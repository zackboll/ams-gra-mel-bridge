"""Event/artifact integrity and failure controls for current development CI."""
import argparse
from collections import Counter
import copy
import contextlib
import io
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest import mock

ROOT = Path(__file__).resolve().parents[2]
sys.path[:0] = [str(ROOT / 'docs/tools'), str(ROOT / 'scripts')]
import documentation_ci as ci
import epoch_ci as e
import feature_source as s
import source_epoch as epoch

class AggregateTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        # Synthetic reports exercise reconciliation, not historical qualification.
        # Real immutable original/replay execution has its own isolated tests.
        shards = {owner: [f'{owner}.Fixture.test_{i}' for i in range(count)]
                  for owner, count in zip(ci.SHARDS, (34, 35, 72, 57, 52, 187))}
        cls.historical_inventory = {
            'groups': {'scripts': [f'scripts.Fixture.test_{i}' for i in range(142)],
                       'enhanced': sorted(x for ids in shards.values() for x in ids)},
            'shards': shards}

    def setUp(self):
        inv = copy.deepcopy(self.historical_inventory)
        self.context = {'source_sha': 'a' * 40, 'source_tree': 'b' * 40,
                        'target_sha': 'c' * 40, 'target_tree': 'd' * 40,
                        'historical_sha': e.HISTORY, 'historical_tree': e.HISTORY_TREE,
                        'event': 'pull_request', 'run_id': '2002', 'run_attempt': '1',
                        'qualification_mode': 'hosted',
                        'repository': 'zackboll/ams-gra-mel-bridge'}
        self.record = {'identity': self.context, 'historical': {'inventory': inv,
                       'seal': {'manifest_sha256': 'e' * 64, 'bundle_sha256': 'f' * 64}}}
        self.qualified = {'artifact_id': '123', 'manifest_sha256': '1' * 64, 'bundle_sha256': '2' * 64}
        self.needs = {n: {'result': 'success'} for n in ('prepare-documentation', 'enhanced-tests')}
        import ci001r4_replay as replay
        history = {'source_sha': replay.COMMIT, 'source_tree': replay.TREE, 'run_id': '2002',
                   'run_attempt': '1', 'repository': self.context['repository'], 'qualification_mode': 'hosted'}
        self.record['historical']['context'] = history
        qualified = {'artifact_id': '123', 'manifest_sha256': 'e' * 64, 'bundle_sha256': 'f' * 64,
                     'artifact_name': ci.artifact_name('inputs', history)}
        self.reports = {}
        for owner, ids in inv['shards'].items():
            execution = {'schema': 1, 'owner': owner, 'identity': history, 'inventory_sha256': ci.digest(inv),
                         'qualified': qualified, 'artifact_name': ci.artifact_name('result-' + owner, history),
                         'successful': True, 'tests_run': len(ids),
                         'events': [{'id': n, 'event': k, **({'seconds': 0.01} if k == 'stop' else {})}
                                    for n in ids for k in ('start', 'success', 'stop')]}
            self.reports[owner] = {'schema': 2, 'owner': owner, 'identity': self.context,
                                   'epochs_sha256': e.digest(self.record), **self.qualified, 'execution': execution}

    def gate(self):
        return e.reconcile(self.record, self.reports, self.qualified, self.needs)

    def test_complete_six_shards_pass_mechanically_not_approval(self):
        gate = self.gate()
        self.assertTrue(gate['mechanically_qualified'])
        self.assertFalse(gate['independent_approval'])
        self.assertFalse(gate['deployable'])
        self.assertEqual(gate['historical_enhanced'], 437)

    def test_missing_shard_rejected(self):
        del self.reports['live']
        with self.assertRaises(ValueError): self.gate()

    def test_mixed_source_tree_rejected(self):
        self.reports['live']['identity'] = {**self.context, 'source_tree': '0' * 40}
        with self.assertRaises(ValueError): self.gate()

    def test_altered_source_run_attempt_and_historical_epoch_rejected(self):
        for field in ('source_sha', 'run_id', 'run_attempt'):
            original = self.reports['live']['identity']
            self.reports['live']['identity'] = {**original, field: '0'}
            with self.assertRaises(ValueError):
                self.gate()
            self.reports['live']['identity'] = original
        original = self.record['historical']['context']
        self.record['historical']['context'] = {**original, 'source_sha': '0' * 40}
        with self.assertRaises(ValueError):
            self.gate()
        self.record['historical']['context'] = original

    def test_wrong_candidate_epoch_rejected(self):
        self.reports['live']['epochs_sha256'] = '0' * 64
        with self.assertRaises(ValueError): self.gate()

    def test_mixed_attempt_rejected(self):
        self.reports['live']['execution']['identity'] = {**self.reports['live']['execution']['identity'], 'run_attempt': '2'}
        with self.assertRaises(ValueError): self.gate()

    def test_mixed_artifact_rejected(self):
        self.reports['live']['artifact_id'] = '456'
        with self.assertRaises(ValueError): self.gate()

    def test_missing_execution_rejected(self):
        self.reports['live']['execution']['events'].pop()
        with self.assertRaises(ValueError): self.gate()

    def test_duplicate_execution_rejected(self):
        self.reports['live']['execution']['events'] *= 2
        with self.assertRaises(ValueError): self.gate()

    def test_failed_skipped_cancelled_pending_jobs_rejected(self):
        for status in ('failure', 'skipped', 'cancelled', None, 'pending'):
            with self.subTest(status=status):
                self.needs['enhanced-tests']['result'] = status
                with self.assertRaises(ValueError): self.gate()

    def test_skipped_test_rejected(self):
        self.reports['live']['execution']['events'][1]['event'] = 'skip'
        with self.assertRaises(ValueError): self.gate()

    def test_policy_report_missing_callback_rejected(self):
        with self.assertRaises(ValueError):
            e.validate_policy({'successful': True, 'tests': ['one'], 'events': []}, ['one'])

    def test_reordered_policy_callbacks_rejected(self):
        events=[{'event':k,'id':'one','seconds':0.01} for k in ('success','start','stop')]
        with self.assertRaises(ValueError):
            e.validate_policy({'successful':True,'tests':['one'],'events':events},['one'])

class FailureDiagnosticsTests(unittest.TestCase):
    """Real failing child processes through run_shard, not success-shaped stubs."""

    @classmethod
    def setUpClass(cls):
        AggregateTests.setUpClass.__func__(cls)

    def setUp(self):
        AggregateTests.setUp(self)
        self.temp = tempfile.TemporaryDirectory(prefix='doc-ci004-negative-')
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.directory = self.root / 'inputs'
        self.directory.mkdir()
        self.args = argparse.Namespace(directory=str(self.directory), owner='live',
            output=str(self.root / 'results/live.json'), diagnostics=str(self.root / 'diagnostics/live'),
            artifact_id='123', bundle_sha='2' * 64, manifest_sha='1' * 64)
        self.exit_status = 23
        self.report_bytes = None
        self.stdout = io.StringIO()
        self.addCleanup(mock.patch.stopall)
        mock.patch.object(e, 'verify', return_value=self.record).start()
        mock.patch.object(e, 'historical_checkout', side_effect=self.checkout).start()
        # The synthetic failing child is not a Git checkout. Real replay
        # construction is exercised separately by HistoricalReplaySourceTests.
        mock.patch.object(e, 'corrected_replay').start()
        mock.patch.object(e, 'verify_candidate').start()

    def checkout(self, root):
        (root / 'docs/tools').mkdir(parents=True)
        # Execute a real unittest failure and retain its real callback identity.
        # This synthetic source is never substituted into the historical epoch.
        (root / 'docs/tools/documentation_ci.py').write_text(
            'import argparse,json,sys,unittest\n'
            'sys.path.insert(0,' + repr(str(ROOT / 'docs/tools')) + ')\n'
            'from documentation_ci import EvidenceResult\n'
            'p=argparse.ArgumentParser();p.add_argument("command");p.add_argument("--output");'
            'a,_=p.parse_known_args()\n'
            'if a.command=="unpack":sys.exit(0)\n'
            'print("complete-log-prefix-" + "x"*16000, flush=True)\n'
            'class DeliberateFailure(unittest.TestCase):\n'
            ' def test_inner_failure_identity(self):self.fail("DOC-CI-004 deliberate inner failure")\n'
            'r=unittest.TextTestRunner(verbosity=2,resultclass=EvidenceResult).run('
            'unittest.defaultTestLoader.loadTestsFromTestCase(DeliberateFailure))\n'
            + ('from pathlib import Path\nPath(a.output).write_bytes(' + repr(self.report_bytes) + ')\n'
               if self.report_bytes is not None else
               'from pathlib import Path\nPath(a.output).write_text(json.dumps('
               '{"successful":r.wasSuccessful(),"events":r.events}))\n')
            + 'sys.exit(' + str(self.exit_status) + ')\n')

    def run_failure(self):
        with contextlib.redirect_stderr(self.stdout), self.assertRaises(SystemExit) as failure:
            e.run_shard(self.args)
        self.assertEqual(failure.exception.code, self.exit_status)
        self.assertIsInstance(failure.exception.__cause__, subprocess.CalledProcessError)
        self.assertEqual(failure.exception.__cause__.returncode, self.exit_status)
        self.assertFalse(Path(self.args.output).exists())

    def test_real_inner_failure_retains_log_identity_status_and_hashes(self):
        self.run_failure()
        root = Path(self.args.diagnostics)
        diagnostic = ci.load(root / 'failure.json')
        self.assertEqual(diagnostic['exit_status'], 23)
        self.assertEqual(diagnostic['identity'], self.context)
        self.assertEqual(diagnostic['historical_identity'], self.record['historical']['context'])
        self.assertEqual(diagnostic['historical_seal'], self.record['historical']['seal'])
        self.assertEqual(diagnostic['artifact_id'], self.args.artifact_id)
        self.assertEqual(diagnostic['bundle_sha256'], self.args.bundle_sha)
        self.assertEqual(diagnostic['manifest_sha256'], self.args.manifest_sha)
        self.assertFalse(diagnostic['successful'])
        self.assertIn('--owner', diagnostic['command'])
        self.assertIn('live', diagnostic['command'])
        for name, sha in diagnostic['files_sha256'].items():
            self.assertEqual(ci.file_sha(root / name), sha)
        execution = ci.load(root / 'execution.json')
        self.assertFalse(execution['successful'])
        self.assertIn({'event': 'failure', 'id': '__main__.DeliberateFailure.test_inner_failure_identity'},
                      execution['events'])
        self.assertIn('complete-log-prefix-', (root / 'historical.log').read_text())
        self.assertIn('DOC-CI-004 deliberate inner failure', self.stdout.getvalue())
        self.assertLess(len(self.stdout.getvalue()), 14000)
        self.assertEqual((root / 'execution.json').read_bytes(),
                         (self.directory / 'live-execution.json').read_bytes())
        del self.reports['live']
        with self.assertRaises(ValueError):
            e.reconcile(self.record, self.reports, self.qualified, self.needs)

    def test_malformed_and_truncated_inner_reports_retained_as_raw_bytes(self):
        for number, data in enumerate((b'{"successful":', b'not-json', b'')):
            with self.subTest(data=data):
                self.report_bytes = data
                self.args.directory = str(self.root / ('inputs-' + str(number)))
                Path(self.args.directory).mkdir()
                self.args.diagnostics = str(self.root / ('diagnostics-' + str(number)))
                self.run_failure()
                self.assertEqual((Path(self.args.diagnostics) / 'execution.json').read_bytes(), data)

    def test_missing_inner_report_does_not_mask_exit_status(self):
        original = self.checkout
        def checkout(root):
            original(root)
            path = root / 'docs/tools/documentation_ci.py'
            path.write_text(path.read_text().replace('Path(a.output).write_text(json.dumps(',
                                                     'print(json.dumps('))
        with mock.patch.object(e, 'historical_checkout', side_effect=checkout):
            self.run_failure()
        self.assertFalse(ci.load(Path(self.args.diagnostics) / 'failure.json')['inner_report_available'])

    def test_diagnostic_write_failure_preserves_original_exit(self):
        with mock.patch.object(e.shutil, 'copyfile', side_effect=OSError('deliberate disk failure')):
            self.run_failure()
        self.assertIn('deliberate disk failure', self.stdout.getvalue())
        self.assertIn('DOC-CI-004 deliberate inner failure', self.stdout.getvalue())

    def test_unexpected_existing_diagnostics_directory_preserves_failure(self):
        destination = Path(self.args.diagnostics)
        destination.mkdir(parents=True)
        sentinel = destination / 'do-not-overwrite'
        sentinel.write_bytes(b'original evidence')
        self.run_failure()
        self.assertEqual(sentinel.read_bytes(), b'original evidence')
        self.assertEqual(list(destination.iterdir()), [sentinel])

    def test_child_setup_and_teardown_errors_preserve_failure(self):
        original = self.checkout
        for method in ('setUpClass', 'tearDownClass'):
            with self.subTest(method=method):
                self.args.directory = str(self.root / method)
                Path(self.args.directory).mkdir()
                self.args.diagnostics = str(self.root / (method + '-diagnostics'))
                def checkout(root):
                    original(root)
                    path = root / 'docs/tools/documentation_ci.py'
                    path.write_text(path.read_text().replace(
                        ' def test_inner_failure_identity(self):self.fail("DOC-CI-004 deliberate inner failure")',
                        ' @classmethod\n def ' + method + '(cls):raise RuntimeError("deliberate ' + method + '")\n'
                        ' def test_inner_failure_identity(self):pass'))
                with mock.patch.object(e, 'historical_checkout', side_effect=checkout):
                    self.run_failure()
                report = ci.load(Path(self.args.diagnostics) / 'execution.json')
                self.assertFalse(report['successful'])
                self.assertTrue(any(v['event'] == 'error' and method in v['id'] for v in report['events']))

    def test_zero_exit_truncated_and_incomplete_callbacks_rejected(self):
        self.exit_status = 0
        incomplete = copy.deepcopy(self.reports['live']['execution'])
        incomplete['events'].pop()
        for number, data in enumerate((b'{"successful":', json.dumps(incomplete).encode())):
            with self.subTest(data=data[:40]):
                self.args.directory = str(self.root / ('zero-' + str(number)))
                Path(self.args.directory).mkdir()
                self.report_bytes = data
                with self.assertRaises(ValueError):
                    e.run_shard(self.args)
                self.assertFalse(Path(self.args.output).exists())

    def test_unpack_failure_preserves_original_status_without_running_tests(self):
        def checkout(root):
            (root / 'docs/tools').mkdir(parents=True)
            (root / 'docs/tools/documentation_ci.py').write_text(
                'import sys\nprint("deliberate unpack failure")\nsys.exit(23)\n')
        with mock.patch.object(e, 'historical_checkout', side_effect=checkout):
            self.run_failure()
        root = Path(self.args.diagnostics)
        diagnostic = ci.load(root / 'failure.json')
        self.assertIn('unpack', diagnostic['command'])
        self.assertFalse(diagnostic['inner_report_available'])
        self.assertFalse((root / 'historical.log').exists())
        self.assertIn('deliberate unpack failure', self.stdout.getvalue())

    def test_diagnostic_symlink_input_is_not_copied(self):
        log = self.root / 'historical.log'
        victim = self.root / 'victim'
        victim.write_text('private bytes')
        log.symlink_to(victim)
        failure = subprocess.CalledProcessError(23, ['historical', 'run'])
        with contextlib.redirect_stderr(self.stdout), self.assertRaisesRegex(ValueError, 'symlink'):
            e.failure_diagnostics(self.args, self.record, failure, log,
                                  self.root / 'unpack.log', self.root / 'execution.json')
        self.assertNotIn('private bytes', self.stdout.getvalue())
        self.assertFalse(Path(self.args.diagnostics).exists())

    def test_symlink_diagnostics_cannot_write_outside_destination(self):
        target = self.root / 'victim'
        target.mkdir()
        Path(self.args.diagnostics).parent.mkdir()
        Path(self.args.diagnostics).symlink_to(target, target_is_directory=True)
        self.run_failure()
        self.assertEqual(list(target.iterdir()), [])

    def test_diagnostics_cannot_change_sealed_inputs(self):
        self.args.diagnostics = str(self.directory / 'diagnostics')
        self.run_failure()
        self.assertFalse(Path(self.args.diagnostics).exists())

    def test_stale_inner_and_canonical_reports_rejected_before_execution(self):
        for path in (self.directory / 'live-execution.json', Path(self.args.output)):
            with self.subTest(path=path):
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text('{}')
                with self.assertRaisesRegex(ValueError, 'result must be fresh'):
                    e.run_shard(self.args)
                path.unlink()

    def test_zero_exit_with_unsuccessful_execution_cannot_create_canonical_report(self):
        self.exit_status = 0
        report = copy.deepcopy(self.reports['live']['execution'])
        report['successful'] = False
        self.report_bytes = json.dumps(report).encode()
        with self.assertRaises(ValueError):
            e.run_shard(self.args)
        self.assertFalse(Path(self.args.output).exists())

    def test_diagnostics_missing_truncated_malformed_cannot_authorize_aggregate(self):
        del self.reports['live']
        for data in (None, b'{"successful":', b'not-json', b'{"successful":true}'):
            with self.subTest(data=data):
                path = self.root / 'failure.json'
                if data is not None: path.write_bytes(data)
                with self.assertRaises(ValueError):
                    e.reconcile(self.record, self.reports, self.qualified, self.needs)

    def test_diagnostic_cannot_substitute_for_canonical_report(self):
        self.run_failure()
        self.reports['live'] = ci.load(Path(self.args.diagnostics) / 'failure.json')
        with self.assertRaises(ValueError):
            e.reconcile(self.record, self.reports, self.qualified, self.needs)

    def test_workflow_upload_is_independent_and_diagnostics_are_not_gate_inputs(self):
        workflow = (ROOT / '.github/workflows/documentation.yml').read_text()
        upload = workflow.split('- name: Upload non-authorizing diagnostics and coverage\n', 1)[1].split('\n  deploy:', 1)[0]
        self.assertIn('if: always()', upload)
        self.assertIn('documentation-diagnostics-', upload)
        self.assertIn('if-no-files-found: warn', upload)
        self.assertNotIn('artifact-ids:', upload)
        self.assertNotIn('pattern:', workflow)
        self.assertIn('needs: [build]', workflow)
        self.assertIn('artifact-ids: ${{ needs.build.outputs.artifact_id }}', workflow)

class ArtifactBoundaryTests(unittest.TestCase):
    def setUp(self):
        import tarfile
        import io
        self.tarfile, self.io = tarfile, io
        self.temp = tempfile.TemporaryDirectory(prefix='pol002-archive-')
        self.addCleanup(self.temp.cleanup)
        self.dir = Path(self.temp.name)

    def archive(self, name, kind=None, duplicate=False):
        manifest = {'schema': 2, 'identity': {}, 'epochs_sha256': 'a' * 64,
                    'payload': {name: 'b' * 64}}
        path = self.dir / 'inputs.tar'
        with self.tarfile.open(path, 'w') as tar:
            data = epoch.canonical(manifest)
            m = self.tarfile.TarInfo('manifest.json');m.size = len(data);tar.addfile(m,self.io.BytesIO(data))
            m = self.tarfile.TarInfo(name)
            if kind:
                m.type = kind;m.linkname = '/etc/passwd'
                tar.addfile(m)
            else:
                m.size=1;tar.addfile(m,self.io.BytesIO(b'x'))
            if duplicate:
                m = self.tarfile.TarInfo(name);m.size=1;tar.addfile(m,self.io.BytesIO(b'x'))
        return argparse.Namespace(bundle=path, bundle_sha=ci.file_sha(path), manifest_sha=e.digest(manifest),
                                  directory=self.dir/'unpacked')

    def test_outer_archive_traversal_rejected(self):
        with self.assertRaises(ValueError): e.unpack(self.archive('../escape'))

    def test_outer_archive_symlink_rejected(self):
        with self.assertRaises(ValueError): e.unpack(self.archive('candidate/link', self.tarfile.SYMTYPE))

    def test_outer_duplicate_member_rejected(self):
        with self.assertRaises(ValueError): e.unpack(self.archive('candidate/data', duplicate=True))

    def test_outer_bundle_digest_mismatch_rejected(self):
        args=self.archive('candidate/data');args.bundle_sha='0'*64
        with self.assertRaises(ValueError): e.unpack(args)

    def test_forged_outer_manifest_digest_rejected(self):
        args=self.archive('candidate/data');args.manifest_sha='0'*64
        with self.assertRaises(ValueError): e.unpack(args)

    def test_local_deployment_rejected(self):
        record={'identity':{'qualification_mode':'local-not-deployable'}}
        with mock.patch.object(e,'verify',return_value=record), mock.patch.dict(os.environ,{
            'GITHUB_EVENT_NAME':'push','GITHUB_REF':'refs/heads/main'}):
            with self.assertRaises(ValueError): e.deployment(argparse.Namespace(directory='unused',manifest_sha='unused'))

    def test_pr_deployment_rejected(self):
        with mock.patch.object(e,'verify',return_value={}), mock.patch.dict(os.environ,{
            'GITHUB_EVENT_NAME':'pull_request','GITHUB_REF':'refs/pull/1/merge'}):
            with self.assertRaises(ValueError): e.deployment(argparse.Namespace(directory='unused',manifest_sha='unused'))

    def test_workflow_preserves_dependency_and_permission_boundaries(self):
        text=(ROOT/'.github/workflows/documentation.yml').read_text()
        self.assertNotIn('pull_request_target:',text)
        self.assertIn('permissions:\n  contents: read\n  actions: read',text)
        from docs_workflow import check
        self.assertEqual(set(check(ROOT)), {'build', 'deploy', 'verify-live'})
        self.assertNotIn('epoch_ci.py', text)
        self.assertIn("if: github.ref == 'refs/heads/main' && github.event_name == 'push'",text)
        prepare=text.split('\n  deploy:\n')[0]
        self.assertLess(prepare.index('docs_ci.py build'),prepare.index('Upload verified same-run inputs'))
        deploy=text.split('\n  deploy:\n')[1].split('\n  verify-live:\n')[0]
        self.assertLess(deploy.index('docs_ci.py unpack --deployment'),deploy.index('actions/upload-pages-artifact'))
        self.assertIn('path: .doc-tools/current/published/',deploy)

    def test_generation_does_not_mutate_source_classification(self):
        evidence={'target_commit':'a'*40,'candidate_commit':'b'*40,'case':'feature-source'}
        original=copy.deepcopy(evidence)
        # Reach a controlled stop after generation's copy boundary. It must leave
        # even nested caller-owned source identity objects untouched.
        with tempfile.TemporaryDirectory() as t, mock.patch.object(epoch,'toolchain',side_effect=ValueError('stop')):
            with self.assertRaises(ValueError):
                epoch.qualify_evidence(ROOT,evidence,Path(t)/'output',Path(t)/'tools')
        self.assertEqual(evidence,original)

class EventAuthorityTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='pol002-event-')
        self.addCleanup(self.temp.cleanup)
        self.directory = Path(self.temp.name)
        self.repo = self.directory / 'source'
        # Detached commit maintenance can recreate .git during temporary cleanup.
        # Set this on the clone before any checkout/commit, never on the source.
        subprocess.run(['git', 'clone', '--shared', '--no-checkout',
                        '--config', 'maintenance.auto=false', str(ROOT), str(self.repo)],
                       check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        self.git('checkout', '--detach', e.HISTORY)
        self.git('config', 'user.name', 'POL002 Event')
        self.git('config', 'user.email', 'pol002@example.invalid')
        path = self.repo / 'docs/event-fixture.md'
        path.write_text('Synthetic read-only PR context.\n')
        self.git('add', '.'); self.git('commit', '-qm', 'event candidate')
        self.head = self.git('rev-parse', 'HEAD')
        self.git('update-ref', 'refs/remotes/origin/main', e.HISTORY)
        self.git('update-ref', 'refs/remotes/origin/pull/2002/head', self.head)
        self.event = {'number': 2002, 'repository': {'full_name': 'zackboll/ams-gra-mel-bridge'},
                      'pull_request': {'base': {'ref': 'main', 'sha': e.HISTORY,
                                               'repo': {'full_name': 'zackboll/ams-gra-mel-bridge'}},
                                       'head': {'sha': self.head}}}
        self.event_file = self.directory / 'event.json'
        self.env = {'GITHUB_EVENT_PATH': str(self.event_file), 'GITHUB_EVENT_NAME': 'pull_request',
                    'GITHUB_REF': 'refs/pull/2002/merge', 'GITHUB_REPOSITORY': 'zackboll/ams-gra-mel-bridge',
                    'GITHUB_RUN_ID': '2002', 'GITHUB_RUN_ATTEMPT': '1'}

    def git(self, *args):
        return subprocess.check_output(['git', '-C', str(self.repo), *args], stderr=subprocess.STDOUT).decode().strip()

    def context(self):
        self.event_file.write_text(json.dumps(self.event))
        with mock.patch.object(e, 'ROOT', self.repo), mock.patch.dict(os.environ, self.env):
            return e.context()

    def test_real_committed_event_and_fetched_head_qualify(self):
        value = self.context()
        self.assertEqual(value['source_sha'], self.head)
        self.assertEqual(value['historical_sha'], e.HISTORY)
        self.assertEqual(value['target_sha'], e.HISTORY)

    def test_fixture_commit_does_not_start_automatic_maintenance(self):
        self.assertEqual(self.git('config', '--local', '--get', 'maintenance.auto'), 'false')
        trace = self.directory / 'commit-trace.json'
        with mock.patch.dict(os.environ, {'GIT_TRACE2_EVENT': str(trace)}):
            self.git('commit', '--allow-empty', '-qm', 'maintenance regression')
        events = [json.loads(line) for line in trace.read_text().splitlines()]
        self.assertTrue(any(event['event'] == 'cmd_name' and event['name'] == 'commit'
                            for event in events))
        self.assertFalse(any(event['event'] == 'child_start' and
                             'maintenance' in event.get('argv', []) for event in events))

    def test_claimed_base_not_on_trusted_main_rejected(self):
        self.event['pull_request']['base']['sha'] = self.head
        with self.assertRaises(ValueError): self.context()

    def test_claimed_head_differs_from_fetched_pr_ref_rejected(self):
        self.event['pull_request']['head']['sha'] = e.HISTORY
        with self.assertRaises(ValueError): self.context()

    def test_wrong_target_repository_rejected(self):
        self.event['pull_request']['base']['repo']['full_name'] = 'attacker/repo'
        with self.assertRaises(ValueError): self.context()

    def test_wrong_ref_rejected(self):
        self.env['GITHUB_REF'] = 'refs/heads/main'
        with self.assertRaises(ValueError): self.context()

    def test_pull_request_target_rejected(self):
        self.env['GITHUB_EVENT_NAME'] = 'pull_request_target'
        with self.assertRaises(ValueError): self.context()

    def test_dirty_checkout_cannot_claim_exact_candidate(self):
        (self.repo / 'docs/event-fixture.md').write_text('changed without commit\n')
        with self.assertRaises(ValueError): self.context()


class MainPublicationTests(unittest.TestCase):
    def test_pr_identity_cannot_authorize_publication(self):
        head = s.revision(ROOT, 'HEAD')
        with mock.patch.dict(os.environ, {'GITHUB_EVENT_NAME': 'pull_request',
                                         'GITHUB_REF': 'refs/pull/142/merge', 'GITHUB_SHA': head}):
            with self.assertRaisesRegex(ValueError, 'main may publish'):
                epoch.publication_authority(ROOT, head, pristine={'a': '1'}, published={'b': '2'})

    def test_non_main_push_cannot_authorize_publication(self):
        head = s.revision(ROOT, 'HEAD')
        with mock.patch.dict(os.environ, {'GITHUB_EVENT_NAME': 'push',
                                         'GITHUB_REF': 'refs/heads/feature', 'GITHUB_SHA': head}):
            with self.assertRaisesRegex(ValueError, 'main may publish'):
                epoch.publication_authority(ROOT, head, pristine={'a': '1'}, published={'b': '2'})

    def test_qualified_main_needs_no_feature_registry(self):
        head = s.revision(ROOT, 'HEAD')
        env = {'GITHUB_EVENT_NAME': 'push', 'GITHUB_REF': 'refs/heads/main', 'GITHUB_SHA': head}
        with mock.patch.dict(os.environ, env):
            first = epoch.publication_authority(ROOT, head, pristine={'a': '1'}, published={'b': '2'})
            second = epoch.publication_authority(ROOT, head, pristine={'a': '1'}, published={'b': '3'})
        self.assertEqual(first['kind'], 'qualified-main-ci')
        self.assertEqual(first['commit'], head)
        self.assertNotEqual(first['published_sha256'], second['published_sha256'])

    def test_wrong_main_event_sha_rejects(self):
        head = s.revision(ROOT, 'HEAD')
        with mock.patch.dict(os.environ, {'GITHUB_EVENT_NAME': 'push',
                                         'GITHUB_REF': 'refs/heads/main', 'GITHUB_SHA': '0' * 40}):
            with self.assertRaises(ValueError):
                epoch.publication_authority(ROOT, head, pristine={'a': '1'}, published={'b': '2'})


class DiagnosticR2FailureControls(unittest.TestCase):
    """Required job failure remains failure after complete forensic capture."""

    setUpClass = classmethod(FailureDiagnosticsTests.setUpClass.__func__)
    setUp = FailureDiagnosticsTests.setUp
    checkout = FailureDiagnosticsTests.checkout
    run_failure = FailureDiagnosticsTests.run_failure

    def test_wrong_owner_and_preexisting_log_symlinks_rejected_before_child(self):
        self.args.owner = '../live'
        with self.assertRaisesRegex(ValueError, 'owner'):
            e.run_shard(self.args)
        self.args.owner = 'live'
        victim = self.root / 'protected'
        victim.write_bytes(b'unchanged')
        for name in ('live.log', 'live-unpack.log'):
            path = self.directory / name
            path.symlink_to(victim)
            with self.assertRaisesRegex(ValueError, 'logs must be fresh'):
                e.run_shard(self.args)
            self.assertEqual(victim.read_bytes(), b'unchanged')
            path.unlink()

    def test_unsafe_source_canonical_and_input_ancestor_destinations_rejected(self):
        failure = subprocess.CalledProcessError(23, ['historical', 'run'])
        for destination in (self.root, Path(self.args.output),
                            ROOT / 'docs/diagnostic-must-not-be-created'):
            self.args.diagnostics = str(destination)
            with contextlib.redirect_stderr(self.stdout), self.assertRaisesRegex(ValueError, 'overlap'):
                e.failure_diagnostics(self.args, self.record, failure,
                                      self.directory / 'live.log', self.directory / 'unpack.log',
                                      self.directory / 'execution.json')

    def test_real_failure_with_success_shaped_report_never_authorizes(self):
        self.report_bytes = json.dumps(self.reports['live']['execution']).encode()
        self.run_failure()
        captured = Path(self.args.diagnostics)
        self.assertEqual((captured / 'execution.json').read_bytes(), self.report_bytes)
        self.assertEqual(ci.load(captured / 'failure.json')['exit_status'], 23)
        self.needs['enhanced-tests']['result'] = 'failure'
        with self.assertRaisesRegex(ValueError, 'prerequisite'):
            e.reconcile(self.record, self.reports, self.qualified, self.needs)

    def test_complete_diagnostics_in_aggregate_directory_cannot_create_gate(self):
        self.run_failure()
        reports = self.root / 'aggregate-inputs'
        reports.mkdir()
        for owner, report in self.reports.items():
            if owner != 'live': e.write(reports / (owner + '.json'), report)
        import shutil
        shutil.copytree(self.args.diagnostics, reports / 'diagnostics')
        args = argparse.Namespace(directory=self.args.directory, manifest_sha=self.args.manifest_sha,
            reports=str(reports), output=str(self.root / 'gate.json'), artifact_id=self.args.artifact_id,
            bundle_sha=self.args.bundle_sha)
        with mock.patch.dict(os.environ, {'DOC_NEEDS': json.dumps(self.needs)}):
            with self.assertRaises((ValueError, KeyError)): e.aggregate(args)
        self.assertFalse(Path(args.output).exists())

    def test_changed_source_or_qualified_artifact_cannot_reuse_success(self):
        for field in ('source_sha', 'source_tree', 'run_id', 'run_attempt'):
            report = copy.deepcopy(self.reports['live'])
            report['identity'][field] = '0' * 40
            self.reports['live'] = report
            with self.assertRaises(ValueError):
                e.reconcile(self.record, self.reports, self.qualified, self.needs)
            self.reports['live']['identity'] = self.context
        for field in ('artifact_id', 'bundle_sha256', 'manifest_sha256'):
            original = self.reports['live'][field]
            self.reports['live'][field] = '0' * 64
            with self.assertRaises(ValueError):
                e.reconcile(self.record, self.reports, self.qualified, self.needs)
            self.reports['live'][field] = original

    def test_real_child_exit_codes_and_workflow_commands_are_not_masked(self):
        original = self.checkout
        def checkout(root):
            original(root)
            path = root / 'docs/tools/documentation_ci.py'
            path.write_text(path.read_text().replace('class DeliberateFailure',
                'print("::error::untrusted\\x1b[31m\\rtext", flush=True)\nclass DeliberateFailure'))
        for status in (1, 7, 63):
            self.exit_status = status
            self.args.directory = str(self.root / ('inputs-exit-' + str(status)))
            Path(self.args.directory).mkdir()
            self.args.diagnostics = str(self.root / ('diagnostics-exit-' + str(status)))
            self.stdout = io.StringIO()
            with mock.patch.object(e, 'historical_checkout', side_effect=checkout):
                self.run_failure()
            self.assertEqual(ci.load(Path(self.args.diagnostics) / 'failure.json')['exit_status'], status)
            excerpt = self.stdout.getvalue()
            self.assertIn(' | ::error::untrusted\\u001b', excerpt)
            self.assertNotIn('\x1b', excerpt)
            self.assertFalse(any(line.startswith('::') for line in excerpt.splitlines()))
            self.assertLess(len(excerpt), 14000)


class DiagnosticIntegrationFailureTests(unittest.TestCase):
    setUpClass = classmethod(FailureDiagnosticsTests.setUpClass.__func__)
    setUp = FailureDiagnosticsTests.setUp
    checkout = FailureDiagnosticsTests.checkout
    run_failure = FailureDiagnosticsTests.run_failure

    def test_each_real_child_status_survives_diagnostic_write_failure(self):
        for status in (1, 7, 63):
            self.exit_status = status
            self.args.directory = str(self.root / ('write-failure-' + str(status)))
            Path(self.args.directory).mkdir()
            self.args.diagnostics = str(self.root / ('diagnostic-' + str(status)))
            with mock.patch.object(e.shutil, 'copyfile', side_effect=RuntimeError('controlled collection failure')):
                self.run_failure()

    def test_traversal_and_sealed_input_outputs_rejected_before_child(self):
        for output in (self.directory / 'historical.tar', self.root / 'new/../output.json',
                       ROOT / 'docs/output-must-not-exist.json'):
            self.args.output = str(output)
            with mock.patch.object(e, 'historical_checkout') as child, self.assertRaises(ValueError):
                e.run_shard(self.args)
            child.assert_not_called()

    def test_diagnostic_path_traversal_does_not_mask_failed_child(self):
        self.args.diagnostics = str(self.root / 'new/../diagnostics')
        self.exit_status = 7
        self.run_failure()
        self.assertFalse((self.root / 'diagnostics').exists())


class HistoricalReplayIdentityTests(unittest.TestCase):
    setUpClass = classmethod(AggregateTests.setUpClass.__func__)
    setUp = AggregateTests.setUp
    gate = AggregateTests.gate

    def test_replay_success_is_mechanical_not_approval(self):
        gate = self.gate()
        self.assertTrue(gate['mechanically_qualified'])
        self.assertFalse(gate['independent_approval'])
        self.assertIsNone(gate['publication_authority'])
        self.assertFalse(gate['deployable'])
        import ci001r4_replay as replay
        self.assertEqual(gate['historical_execution'], {
            'status': 'corrected-replay-not-original-historical-execution',
            'publication_commit': e.HISTORY, 'replay_commit': replay.COMMIT, 'replay_tree': replay.TREE})

    def test_original_epoch_cannot_be_relabelled_as_replay_execution(self):
        self.record['historical']['context'] = {**self.record['historical']['context'],
                                                'source_sha': e.HISTORY, 'source_tree': e.HISTORY_TREE}
        with self.assertRaisesRegex(ValueError, 'historical execution epoch'):
            self.gate()


class RunnerLifecycleTests(unittest.TestCase):
    def test_class_teardown_error_cannot_be_a_successful_report(self):
        class BrokenTeardown(unittest.TestCase):
            @classmethod
            def tearDownClass(cls):
                raise RuntimeError('controlled teardown failure')

            def test_body(self):
                pass

        result = unittest.TextTestRunner(stream=io.StringIO(), resultclass=ci.EvidenceResult).run(
            unittest.defaultTestLoader.loadTestsFromTestCase(BrokenTeardown))
        self.assertFalse(result.wasSuccessful())
        self.assertEqual(result.testsRun, 1)
        self.assertEqual([event['event'] for event in result.events], ['start', 'success', 'stop', 'error'])
        report = {'successful': result.wasSuccessful(), 'tests': ['one'], 'events': result.events}
        with self.assertRaises(ValueError):
            e.validate_policy(report, ['one'])


class PublicationGateTests(unittest.TestCase):
    def setUp(self):
        self.identity = {'event': 'push', 'qualification_mode': 'hosted',
                         'source_sha': 'a' * 40, 'source_tree': 'b' * 40}
        self.record = {'identity': self.identity}
        self.qualified = {'artifact_id': '1', 'bundle_sha256': 'c' * 64, 'manifest_sha256': 'd' * 64}
        self.authority = {'kind': 'qualified-main-ci', 'commit': 'a' * 40}
        self.gate = {'mechanically_qualified': True, 'deployable': True,
                     'identity': self.identity, 'epochs_sha256': e.digest(self.record),
                     'qualified': self.qualified, 'report_sha256': {key: 'e' * 64 for key in ci.SHARDS},
                     'publication_authority': self.authority}
        self.args = argparse.Namespace(directory='unused', manifest_sha='d' * 64,
                                       gate='unused', gate_sha='f' * 64, artifact_id='1',
                                       bundle_sha='c' * 64)
        self.addCleanup(mock.patch.stopall)
        mock.patch.object(e, 'verify', return_value=self.record).start()
        mock.patch.object(e, 'context', return_value=self.identity).start()
        mock.patch.object(ci, 'file_sha', return_value='f' * 64).start()
        mock.patch.object(ci, 'load', return_value=self.gate).start()
        self.reconstruct = mock.patch.object(e, 'verify_candidate').start()
        self.decision = mock.patch.object(e, 'publication_decision', return_value=self.authority).start()
        mock.patch.dict(os.environ, {'GITHUB_EVENT_NAME': 'push', 'GITHUB_REF': 'refs/heads/main'}).start()

    def test_main_gate_reconstructs_candidate_before_deployment(self):
        e.deployment(self.args)
        self.reconstruct.assert_called_once()

    def test_passing_flags_cannot_bypass_recomputed_publication_identity(self):
        self.decision.return_value = None
        with self.assertRaisesRegex(ValueError, 'publication identity'):
            e.deployment(self.args)
        self.reconstruct.assert_not_called()

    def test_source_and_artifact_gate_substitution_rejected(self):
        mutations = {'identity': {**self.identity, 'source_tree': '0' * 40},
                     'epochs_sha256': '0' * 64,
                     'qualified': {**self.qualified, 'artifact_id': '2'},
                     'publication_authority': {'kind': 'forged'},
                     'report_sha256': {}, 'deployable': False}
        for field, changed in mutations.items():
            with self.subTest(field=field):
                original = self.gate[field]
                self.gate[field] = changed
                with self.assertRaises(ValueError):
                    e.deployment(self.args)
                self.gate[field] = original


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('-v', action='store_true')
    parser.add_argument('--report', type=Path)
    args = parser.parse_args()
    suite = unittest.defaultTestLoader.loadTestsFromModule(sys.modules[__name__])
    ids = sorted(t.id() for t in ci.flatten(suite))
    result = unittest.TextTestRunner(verbosity=2 if args.v else 1, resultclass=ci.EvidenceResult).run(suite)
    report = {'successful': result.wasSuccessful(), 'tests': ids, 'events': result.events}
    try:
        e.validate_policy(report, ids)
    except ValueError:
        report['successful'] = False
    if args.report: e.write(args.report, report)
    sys.exit(0 if report['successful'] else 1)
