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

    def test_archive_missing_manifest_inventory_and_manifest_digest_rejected(self):
        bundle, digest = self.archive([('page', b'content', tarfile.REGTYPE)])
        with self.assertRaisesRegex(ValueError, 'missing archive manifest'):
            ci.unpack(bundle, self.root / 'unpacked', digest, '0' * 64)
        raw = ci.epoch.canonical({'payload': {'missing': ci.epoch.sha(b'page')}})
        bundle, digest = self.archive([('manifest.json', raw, tarfile.REGTYPE)])
        with self.assertRaisesRegex(ValueError, 'archive manifest mismatch'):
            ci.unpack(bundle, self.root / 'unpacked', digest, '0' * 64)
        with self.assertRaisesRegex(ValueError, 'archive inventory mismatch'):
            ci.unpack(bundle, self.root / 'unpacked', digest, ci.epoch.sha(raw))

    def test_archive_symlink_output_ancestor_and_existing_directory_rejected(self):
        raw = ci.epoch.canonical({'payload': {}})
        bundle, digest = self.archive([('manifest.json', raw, tarfile.REGTYPE)])
        link = self.root / 'alias'
        link.symlink_to(self.repo, target_is_directory=True)
        for destination in (link / 'unpacked', self.repo):
            with self.subTest(destination=destination), self.assertRaisesRegex(ValueError, 'fresh'):
                ci.unpack(bundle, destination, digest, ci.epoch.sha(raw))

    def test_symlink_payload_rejected(self):
        directory, digest = self.sealed()
        (directory / 'published/alias').symlink_to(directory / 'published/index.html')
        with self.assertRaisesRegex(ValueError, 'symlink'):
            ci.verify(self.repo, directory, digest)

    def test_hosted_pr_rejects_another_repository_destination(self):
        self.hosted('pull_request', 'refs/pull/1/merge')
        path = Path(os.environ['GITHUB_EVENT_PATH'])
        event = json.loads(path.read_bytes())
        event['pull_request']['base']['repo']['full_name'] = 'another/repo'
        ci.write(path, event)
        with self.assertRaisesRegex(ValueError, 'untrusted PR'):
            ci.identity(self.repo)

    def test_warning_classification_failure_is_retained_not_a_gate(self):
        # Exercise actual generation control flow with isolated fake tool output;
        # the real GNATdoc source-addition case is in explicit source qualification.
        from compare_gnatdoc_warnings import WarningComparisonError
        import classify_gnatdoc_warnings
        import gnatdoc_inventory
        import check_doc_coverage
        import build_docs_portal
        import ada_examples
        import validate_docs
        exported = self.root / 'export'

        def export(*args):
            exported.mkdir()

        with mock.patch.object(source_epoch, 'export', side_effect=export), \
             mock.patch.object(gnatdoc_inventory, 'specifications', return_value=({}, {})), \
             mock.patch.object(source_epoch, 'run'), \
             mock.patch.object(source_epoch.subprocess, 'run', return_value=mock.Mock(
                 returncode=0, stdout='new warning format\n', stderr='')), \
             mock.patch.object(classify_gnatdoc_warnings, 'classify',
                               side_effect=WarningComparisonError('unmapped diagnostic')), \
             mock.patch.object(check_doc_coverage, 'measure', return_value={'native': {}}), \
             mock.patch.object(ada_examples, 'References'), \
             mock.patch.object(build_docs_portal, 'manifest', return_value={
                 'home': 'index.html', 'guides': [], 'stylesheet': 'style.css'}), \
             mock.patch.object(build_docs_portal, 'package_map', return_value={}), \
             mock.patch.object(build_docs_portal, 'regular', return_value='text'), \
             mock.patch.object(build_docs_portal, 'render', return_value='page'), \
             mock.patch.object(validate_docs, 'validate'):
            source_epoch.generate(self.repo, self.sha, exported, self.root, self.root / 'generation.log')
        value = json.loads((exported / 'obj/docs/warnings.json').read_bytes())
        self.assertEqual(value['classification_error'], 'unmapped diagnostic')
        self.assertEqual((exported / 'obj/docs/gnatdoc.log').read_text(), 'new warning format\n')

    def test_real_build_failure_and_gnatdoc_errors_cannot_reach_coverage(self):
        import gnatdoc_inventory
        import check_doc_coverage

        def export(root, commit, destination):
            destination.mkdir()

        for number, (status, text) in enumerate(((1, ''), (0, 'error: failed'), (0, 'fatal: failed'),
                                                (0, 'ignore file due to parse failure'))):
            with self.subTest(status=status, text=text):
                destination = self.root / ('export-' + str(number))
                with mock.patch.object(source_epoch, 'export', side_effect=export), \
                     mock.patch.object(gnatdoc_inventory, 'specifications', return_value=({}, {})), \
                     mock.patch.object(source_epoch, 'run'), \
                     mock.patch.object(source_epoch.subprocess, 'run', return_value=mock.Mock(
                         returncode=status, stdout=text, stderr='')), \
                     mock.patch.object(check_doc_coverage, 'measure') as measure:
                    with self.assertRaisesRegex(ValueError, 'incomplete native generation'):
                        source_epoch.generate(self.repo, self.sha, destination, self.root,
                                              self.root / 'generation.log')
                    measure.assert_not_called()

        with mock.patch.object(source_epoch, 'export', side_effect=export), \
             mock.patch.object(gnatdoc_inventory, 'specifications', return_value=({}, {})), \
             mock.patch.object(source_epoch, 'run', side_effect=ValueError('compilation failed')), \
             mock.patch.object(source_epoch.subprocess, 'run') as gnatdoc:
            with self.assertRaisesRegex(ValueError, 'compilation failed'):
                source_epoch.generate(self.repo, self.sha, self.root / 'compile-export', self.root,
                                      self.root / 'generation.log')
            gnatdoc.assert_not_called()

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
