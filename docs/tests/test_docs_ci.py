"""Current-source pipeline identity, archive and security regression controls."""

import io
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tarfile
import tempfile
import unittest
from unittest import mock

ROOT = Path(__file__).resolve().parents[2]
sys.path[:0] = [str(ROOT / 'docs/tools'), str(ROOT / 'scripts')]
import docs_ci as ci
import docs_workflow
import source_epoch


class PipelineTests(unittest.TestCase):
    def setUp(self):
        temp = tempfile.TemporaryDirectory()
        self.addCleanup(temp.cleanup)
        self.root = Path(temp.name)
        self.repo = self.root / 'repo'
        self.repo.mkdir()
        self.git('init', '-q')
        self.git('config', 'user.name', 'Documentation Fixture')
        self.git('config', 'user.email', 'fixture@example.invalid')
        (self.repo / 'source.ads').write_text('package Source is end Source;\n')
        self.git('add', '.')
        self.git('commit', '-qm', 'fixture')
        self.sha = self.git('rev-parse', 'HEAD').strip()
        self.environment = mock.patch.dict(os.environ, {}, clear=True)
        self.environment.start()
        self.addCleanup(self.environment.stop)

    def git(self, *args):
        return subprocess.check_output(['git', '-C', str(self.repo), *args], text=True)

    def hosted(self, kind='push', ref='refs/heads/main'):
        event = {'repository': {'full_name': 'owner/repo'}, 'ref': ref, 'after': self.sha}
        if kind == 'pull_request':
            event.update(number=1, pull_request={'head': {'sha': self.sha},
                         'base': {'ref': 'main', 'repo': {'full_name': 'owner/repo'}}})
        path = self.root / 'event.json'
        ci.write(path, event)
        os.environ.update(GITHUB_ACTIONS='true', GITHUB_EVENT_NAME=kind, GITHUB_REF=ref,
                          GITHUB_SHA=self.sha, DOC_SOURCE_SHA=self.sha,
                          GITHUB_REPOSITORY='owner/repo', GITHUB_EVENT_PATH=str(path),
                          GITHUB_RUN_ID='123', GITHUB_RUN_ATTEMPT='2')

    def sealed(self):
        directory = self.root / 'artifact'
        directory.mkdir()
        (directory / 'published').mkdir()
        (directory / 'published/index.html').write_text('<html>fixture</html>')
        value = {'schema': 1, 'identity': ci.identity(self.repo),
                 'payload': ci.presentation.hashes(ci.presentation.files(directory))}
        ci.write(directory / 'manifest.json', value)
        return directory, ci.epoch.sha((directory / 'manifest.json').read_bytes())

    def test_local_identity_is_not_deployment_authority(self):
        directory, digest = self.sealed()
        with self.assertRaisesRegex(ValueError, 'main pushes'):
            ci.verify(self.repo, directory, digest, deployment=True)

    def test_hosted_identity_binds_commit_tree_repository_run_attempt_and_event(self):
        self.hosted()
        value = ci.identity(self.repo)
        self.assertEqual(value['commit'], self.sha)
        self.assertEqual(value['tree'], self.git('rev-parse', 'HEAD^{tree}').strip())
        self.assertEqual((value['run_id'], value['run_attempt']), ('123', '2'))
        self.assertEqual(value['repository'], 'owner/repo')

    def test_wrong_event_commit_rejected(self):
        self.hosted()
        os.environ['DOC_SOURCE_SHA'] = '0' * 40
        with self.assertRaisesRegex(ValueError, 'event source'):
            ci.identity(self.repo)

    def test_pr_and_manual_artifacts_cannot_deploy(self):
        for kind, ref in (('pull_request', 'refs/pull/1/merge'),
                          ('workflow_dispatch', 'refs/heads/main'), ('push', 'refs/heads/feature')):
            with self.subTest(event=kind, ref=ref):
                self.hosted(kind, ref)
                directory, digest = self.sealed()
                with self.assertRaisesRegex(ValueError, 'main pushes'):
                    ci.verify(self.repo, directory, digest, deployment=True)
                shutil.rmtree(directory)

    def test_wrong_run_attempt_rejected(self):
        self.hosted()
        directory, digest = self.sealed()
        os.environ['GITHUB_RUN_ATTEMPT'] = '3'
        with self.assertRaisesRegex(ValueError, 'source/run identity'):
            ci.verify(self.repo, directory, digest)

    def test_wrong_manifest_digest_rejected(self):
        directory, digest = self.sealed()
        with self.assertRaisesRegex(ValueError, 'manifest digest'):
            ci.verify(self.repo, directory, '0' * 64)

    def test_changed_missing_and_extra_payload_rejected(self):
        directory, digest = self.sealed()
        path = directory / 'published/index.html'
        for mutation in ('change', 'remove', 'extra'):
            with self.subTest(mutation=mutation):
                path.write_text('modified') if mutation == 'change' else None
                path.unlink() if mutation == 'remove' else None
                (directory / 'extra').write_text('unlisted') if mutation == 'extra' else None
                with self.assertRaisesRegex(ValueError, 'payload mismatch'):
                    ci.verify(self.repo, directory, digest)

    def test_dirty_index_untracked_and_source_changes_rejected(self):
        (self.repo / 'source.ads').write_text('modified\n')
        with self.assertRaisesRegex(ValueError, 'working tree differs'):
            ci.identity(self.repo)
        self.git('add', '.')
        with self.assertRaisesRegex(ValueError, 'index differs'):
            ci.identity(self.repo)

    def archive(self, entries):
        path = self.root / 'inputs.tar'
        with tarfile.open(path, 'w') as archive:
            for name, data, kind in entries:
                member = tarfile.TarInfo(name)
                member.type = kind
                member.size = len(data)
                archive.addfile(member, io.BytesIO(data))
        return path, ci.epoch.sha(path.read_bytes())

    def test_archive_valid_roundtrip(self):
        raw = ci.epoch.canonical({'payload': {'published/index.html': ci.epoch.sha(b'page')}})
        bundle, digest = self.archive([('manifest.json', raw, tarfile.REGTYPE),
                                       ('published/index.html', b'page', tarfile.REGTYPE)])
        destination = self.root / 'unpacked'
        ci.unpack(bundle, destination, digest, ci.epoch.sha(raw))
        self.assertEqual((destination / 'published/index.html').read_bytes(), b'page')

    def test_archive_traversal_links_duplicates_missing_manifest_and_bad_digest_rejected(self):
        for name, kind in (('../escape', tarfile.REGTYPE), ('/escape', tarfile.REGTYPE),
                           ('published/link', tarfile.SYMTYPE), ('published/link', tarfile.LNKTYPE),
                           ('published//alias', tarfile.REGTYPE)):
            with self.subTest(name=name, kind=kind):
                bundle, digest = self.archive([(name, b'', kind)])
                with self.assertRaisesRegex(ValueError, 'unsafe archive'):
                    ci.unpack(bundle, self.root / 'unpacked', digest, '0' * 64)
        bundle, digest = self.archive([('x', b'', tarfile.REGTYPE)] * 2)
        with self.assertRaisesRegex(ValueError, 'duplicate'):
            ci.unpack(bundle, self.root / 'unpacked', digest, '0' * 64)
        with self.assertRaisesRegex(ValueError, 'bundle digest'):
            ci.unpack(bundle, self.root / 'unpacked', '0' * 64, '0' * 64)

    def test_current_workflow_security_contract(self):
        self.assertEqual(set(docs_workflow.check(ROOT)), {'build', 'deploy', 'verify-live'})

    def test_normal_generation_has_no_baseline_or_repeated_generation(self):
        import inspect
        body = inspect.getsource(source_epoch.generate)
        for forbidden in ('ratchet(', 'compare_gnatdoc_warnings.py', 'gnatdoc-warning-baseline',
                          'gnatdoc-coverage-baseline'):
            self.assertNotIn(forbidden, body)
        self.assertEqual(inspect.getsource(ci.build).count('epoch.generate('), 1)
        self.assertIn('deep=False', inspect.getsource(ci.build))


if __name__ == '__main__':
    unittest.main()
