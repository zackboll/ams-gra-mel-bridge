"""Independent epoch/event/aggregate and representative feature-surface controls."""

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
import test_actions_dependencies as dependencies


class ActionDependencyWorkflowTests(dependencies.DependencyWorkflowTests):
    """Migration preservation controls join the sealed integration inventory."""


class ActionDependencyAuthorizationTests(dependencies.DependencyAuthorizationTests):
    """Independent atomic workflow transition negative controls."""


class ActionDependencyEpochTests(unittest.TestCase):
    def setUp(self):
        self.old = {n: ('100644', 'old') for n, old, _ in e.ACTIONS_SOURCE if old is not None}
        self.new = {n: ('100644', 'new') for n, _, _ in e.ACTIONS_SOURCE}

    def blob(self, root, revision, name):
        if revision == 'old':
            return dependencies.original(name)
        # source.blob is mocked by these controls: read immutable main directly
        # rather than recursively invoking the mocked function.
        return subprocess.check_output(['git', '-C', str(ROOT), 'show',
                                        dependencies.MIGRATION + ':' + name])

    def test_exact_proposal_does_not_approve_itself(self):
        with mock.patch.object(s, 'blob', side_effect=self.blob):
            self.assertEqual(e.actions_delta(ROOT, 'old', 'new', self.old, self.new), set(self.new))

    def test_unknown_bytes_in_every_candidate_and_predecessor_rejected(self):
        for name, predecessor, _ in e.ACTIONS_SOURCE:
            for revision in (('old', 'new') if predecessor else ('new',)):
                def bad(root, ref, path):
                    return self.blob(root, ref, path) + (b'# unauthorized\n' if (ref, path) == (revision, name) else b'')
                with mock.patch.object(s, 'blob', side_effect=bad), self.assertRaisesRegex(ValueError, 'CI-ACTIONS-001'):
                    e.actions_delta(ROOT, 'old', 'new', self.old, self.new)

    def test_missing_executable_and_symlink_modes_rejected(self):
        for name in self.new:
            for mode in (None, '100755', '120000'):
                entries = dict(self.new)
                if mode is None:
                    entries.pop(name)
                else:
                    entries[name] = (mode, 'new')
                with mock.patch.object(s, 'blob', side_effect=self.blob), self.assertRaisesRegex(ValueError, 'CI-ACTIONS-001'):
                    e.actions_delta(ROOT, 'old', 'new', self.old, entries)


class DiagnosticSuccessorTests(dependencies.DependencyAuthorizationTests):
    """Retain every migration control and add exact successor parity checks."""

    def test_diagnostic_successor_requires_complete_node24_pair(self):
        self.stage({name: (ROOT / name).read_bytes() for name in self.old})
        self.assertEqual(self.state(), 'diagnostics_proposed_pending_independent_review')
        for name in self.old:
            self.stage({n: (ROOT / n).read_bytes() for n in self.old})
            self.stage({name: self.old[name]})
            with self.assertRaisesRegex(ValueError, 'mixed or partially'):
                self.state()

    def test_integrated_workflow_is_exact_diagnostic_successor(self):
        name = '.github/workflows/documentation.yml'
        predecessor = s.blob(ROOT, dependencies.MIGRATION, name).decode()
        self.assertEqual((ROOT / name).read_text(), dependencies.diagnostics_successor(predecessor))
        text = (ROOT / name).read_text()
        canonical = text.split('- name: Upload successful canonical execution result\n', 1)[1].split(
            '- name: Upload separate non-authorizing failure diagnostics\n', 1)[0]
        self.assertNotIn('if: always()', canonical)
        for prefix in ('documentation-result-', 'documentation-failure-diagnostics-'):
            self.assertIn('name: ' + prefix + '${{ matrix.shard }}-${{ github.run_id }}-${{ github.run_attempt }}-${{ env.DOC_SOURCE_SHA }}', text)

    def test_exact_source_transition_and_every_partial_or_unauthorized_file(self):
        old = {n: ('100644', 'old') for n, _ in e.DIAGNOSTIC_SOURCE}
        new = {n: ('100644', 'new') for n, _ in e.DIAGNOSTIC_SOURCE}
        original_blob = s.blob
        def blob(root, revision, name):
            return original_blob(ROOT, dependencies.MIGRATION, name) if revision == 'old' else (ROOT / name).read_bytes()
        with mock.patch.object(e.source, 'blob', side_effect=blob):
            self.assertEqual(e.diagnostics_delta(ROOT, 'old', 'new', old, new), set(old))
        for name, _ in e.DIAGNOSTIC_SOURCE:
            for revision in ('old', 'new'):
                def bad(root, ref, path):
                    data = blob(root, ref, path)
                    return data + b'# unauthorized\n' if (ref, path) == (revision, name) else data
                with mock.patch.object(e.source, 'blob', side_effect=bad):
                    if (revision, name) == ('old', '.github/workflows/documentation.yml'):
                        self.assertEqual(e.diagnostics_delta(ROOT, 'old', 'new', old, new), set())
                    else:
                        with self.assertRaisesRegex(ValueError, 'DOC-CI-004R1'):
                            e.diagnostics_delta(ROOT, 'old', 'new', old, new)
            for mode in ('100755', '120000', None):
                entries = dict(new)
                if mode is None:
                    entries.pop(name)
                else:
                    entries[name] = (mode, 'new')
                with mock.patch.object(e.source, 'blob', side_effect=blob), self.assertRaises(ValueError):
                    e.diagnostics_delta(ROOT, 'old', 'new', old, entries)
            def partial(root, revision, path):
                return blob(root, 'old' if path == name else revision, path)
            if name != '.github/workflows/ci.yml':  # Bootstrap intentionally does not change.
                with mock.patch.object(e.source, 'blob', side_effect=partial), self.assertRaises(ValueError):
                    e.diagnostics_delta(ROOT, 'old', 'new', old, new)


class AggregateTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp = tempfile.TemporaryDirectory(prefix='pol002-historical-inventory-')
        cls.addClassCleanup(cls.temp.cleanup)
        historical = Path(cls.temp.name) / 'source'
        e.historical_checkout(historical)
        subprocess.run([sys.executable, 'docs/tools/documentation_ci.py', 'inventory'], cwd=historical,
                       check=True, stdin=subprocess.DEVNULL, stdout=subprocess.DEVNULL)
        cls.historical_inventory = ci.load(historical / 'qualification/inventory.json')

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
        history = {'source_sha': e.HISTORY, 'source_tree': e.HISTORY_TREE, 'run_id': '2002',
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
        upload = workflow.split('- name: Upload separate non-authorizing failure diagnostics\n', 1)[1].split('\n  #', 1)[0]
        self.assertIn('if: always()', upload)
        self.assertIn('uses: actions/upload-artifact@b7c566a772e6b6bfb58ed0dc250532a479d7789f # v6.0.0', upload)
        self.assertIn('name: documentation-failure-diagnostics-', upload)
        self.assertIn('path: ${{ runner.temp }}/failure-diagnostics/${{ matrix.shard }}/', upload)
        self.assertIn('pattern: documentation-result-*', workflow)
        self.assertNotIn('pattern: documentation-failure-diagnostics-', workflow)
        self.assertIn('needs: [prepare-documentation, build]', workflow)
        self.assertIn("all(j['result'] == 'success' for j in jobs.values())", workflow)


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


class DiagnosticR2SourceControls(unittest.TestCase):
    """Real committed predecessor/successor authority; no mocked Git blobs."""

    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='doc-ci004-r2-git-')
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        subprocess.run(['git', 'clone', '--shared', '--no-checkout', str(ROOT), str(self.root)],
                       check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        self.git('checkout', '--detach', dependencies.MIGRATION)
        self.git('config', 'user.name', 'DOC-CI-004 committed control')
        self.git('config', 'user.email', 'doc-ci004@example.invalid')
        self.base = self.git('rev-parse', 'HEAD')
        import doc_ci004_source as integration
        self.names = [*integration.SUCCESSOR, integration.POLICY]
        for name in self.names:
            (self.root / name).parent.mkdir(parents=True, exist_ok=True)
            (self.root / name).write_bytes((ROOT / name).read_bytes())
        self.head = self.commit()

    def git(self, *args):
        return subprocess.check_output(['git', '-C', str(self.root), *args],
                                       stderr=subprocess.STDOUT).decode().strip()

    def commit(self):
        self.git('add', '.'); self.git('commit', '-qm', 'committed diagnostic control')
        return self.git('rev-parse', 'HEAD')

    def classify(self):
        return e.classify(self.root, self.base, self.git('rev-parse', 'HEAD'))

    def test_exact_successor_does_not_self_approve(self):
        value = self.classify()
        self.assertEqual(value['case'], 'policy-only')
        self.assertFalse(value['independent_approval'])
        self.assertFalse(value['publication_epoch_approved'])
        self.assertEqual(e.diagnostics_delta(self.root, self.base, self.head,
            s.tree(self.root, self.base), s.tree(self.root, self.head)),
            {name for name, _ in e.DIAGNOSTIC_SOURCE})

    def test_every_incomplete_unknown_and_nonregular_successor_rejects(self):
        for name, _ in e.DIAGNOSTIC_SOURCE:
            if name.endswith('/ci.yml'): continue  # Intentionally unchanged.
            for mutation in ('predecessor', 'unknown', 'missing', 'executable'):
                with self.subTest(name=name, mutation=mutation):
                    path = self.root / name
                    if mutation == 'predecessor': path.write_bytes(s.blob(ROOT, self.base, name))
                    elif mutation == 'unknown': path.write_bytes(path.read_bytes() + b'\n# unknown\n')
                    elif mutation == 'missing': path.unlink()
                    else: path.chmod(0o755)
                    self.commit()
                    with self.assertRaises(ValueError): self.classify()
                    self.git('reset', '--hard', self.head)

    def test_candidate_registry_and_production_changes_reject(self):
        for name in ('docs/tools/source_epoch.py', 'native/src/ir_stream.cpp'):
            path = self.root / name
            path.write_bytes(path.read_bytes() + b'\n# unreviewed self approval\n')
            self.commit()
            with self.assertRaises(ValueError): self.classify()
            self.git('reset', '--hard', self.head)

    def test_workflow_pin_role_hidden_files_and_gate_input_mutations_reject(self):
        import re
        name = '.github/workflows/documentation.yml'
        text = (self.root / name).read_text()
        refs = list(re.finditer(r'uses: (actions/[^\n]+)', text))
        mutations = []
        for match in refs:
            old = match.group(1)
            role = 'upload-artifact' if old.startswith('actions/checkout@') else 'checkout'
            sha, release, _ = dependencies.PINS[role]
            mutations += [text[:match.start(1)] + 'actions/' + role + '@' + sha + ' # ' + release
                          + text[match.end(1):],
                          text[:match.start(1)] + re.sub(r'@[0-9a-f]{40}', '@' + '0' * 40, old)
                          + text[match.end(1):]]
        mutations += [text.replace('include-hidden-files: true', 'include-hidden-files: false'),
                      text.replace('          include-hidden-files: true\n', ''),
                      text.replace('pattern: documentation-result-*',
                                   'pattern: documentation-failure-diagnostics-*'),
                      text.replace('name: documentation-result-${{ matrix.shard }}',
                                   'name: diagnostic-substitute-${{ matrix.shard }}')]
        for number, changed in enumerate(mutations):
            with self.subTest(mutation=number):
                with self.assertRaises(ValueError): dependencies.validate_workflow(name, changed)
                (self.root / name).write_text(changed); self.commit()
                with self.assertRaises(ValueError): self.classify()
                self.git('reset', '--hard', self.head)


class CurrentWorkflowSuccessorTests(unittest.TestCase):
    """Actual POL-002 workflow boundaries; never replace frozen assertions."""

    def setUp(self):
        import re
        self.text = (ROOT / '.github/workflows/documentation.yml').read_text()
        self.jobs = {name: re.split(r'\n  [a-z][a-z-]+:\n',
            self.text.split('\n  ' + name + ':\n', 1)[1], maxsplit=1)[0]
            for name in ('prepare-documentation', 'enhanced-tests', 'build', 'deploy', 'verify-live')}

    def test_main_only_publication_and_read_only_preparation_verification(self):
        for name in ('deploy', 'verify-live'):
            self.assertIn("if: github.ref == 'refs/heads/main' && github.event_name != 'pull_request'", self.jobs[name])
        self.assertIn('permissions:\n  contents: read\n  actions: read', self.text)
        for name in ('prepare-documentation', 'enhanced-tests', 'build', 'verify-live'):
            self.assertNotIn(': write', self.jobs[name])
            self.assertNotIn('secrets.', self.jobs[name])
            self.assertNotIn('actions/upload-pages-artifact', self.jobs[name])
            self.assertNotIn('actions/deploy-pages', self.jobs[name])

    def test_exact_same_run_inputs_six_shards_and_aggregate_deployment_gate(self):
        for name in ('enhanced-tests', 'build', 'deploy', 'verify-live'):
            job = self.jobs[name]
            self.assertIn('artifact-ids: ${{ needs.prepare-documentation.outputs.artifact_id }}', job)
            self.assertNotIn('run-id:', job)
            self.assertIn("--bundle-sha '${{ needs.prepare-documentation.outputs.bundle_sha }}'", job)
            self.assertIn("--manifest-sha '${{ needs.prepare-documentation.outputs.manifest_sha }}'", job)
        self.assertIn('shard: [navigation, publication, live, symbols-catalog, types, remaining]', self.jobs['enhanced-tests'])
        self.assertIn('fail-fast: false', self.jobs['enhanced-tests'])
        self.assertIn('if: always()', self.jobs['build'])
        self.assertIn("all(j['result'] == 'success' for j in jobs.values())", self.jobs['build'])
        self.assertIn("' aggregate --root", self.jobs['build'])
        self.assertIn('pattern: documentation-result-*-${{ github.run_id }}-${{ github.run_attempt }}-${{ env.DOC_SOURCE_SHA }}', self.jobs['build'])
        for name in ('deploy', 'verify-live'):
            self.assertIn('artifact-ids: ${{ needs.build.outputs.gate_id }}', self.jobs[name])
            self.assertIn("' deployment --root", self.jobs[name])
            self.assertIn("--gate-sha '${{ needs.build.outputs.gate_sha }}'", self.jobs[name])
        self.assertIn('needs: [prepare-documentation, build]', self.jobs['deploy'])
        self.assertIn('needs: [prepare-documentation, build, deploy]', self.jobs['verify-live'])
        self.assertLess(self.jobs['deploy'].index("' deployment --root"),
                        self.jobs['deploy'].index('actions/upload-pages-artifact'))

    def test_historical_workflow_assertions_and_inventory_bytes_unchanged(self):
        import ci001r3_source as correction
        restored = correction.inherited_sources(ROOT, lambda rev, name: s.blob(ROOT, rev, name))
        for name in ('docs/tests/test_live_docs_site.py', 'docs/tests/test_documentation_ci.py',
                     'docs/tests/ci001-inherited-inventory.json'):
            self.assertEqual(restored.get(name, (ROOT / name).read_bytes()),
                s.blob(ROOT, 'e4e756aafa4edaffdb11fe341bb016380bf19ca9', name))
        historical = s.blob(ROOT, e.HISTORY, '.github/workflows/documentation.yml')
        self.assertIn(b'documentation_ci.py deployment', historical)
        self.assertIn(b'documentation_ci.py aggregate', historical)

    def test_class_setup_and_teardown_errors_are_not_test_success(self):
        class SetupError(unittest.TestCase):
            @classmethod
            def setUpClass(cls):
                raise RuntimeError('DOC-CI-004 controlled class setup error')
            def test_not_run(self):
                self.fail('must not execute')
        class TeardownError(unittest.TestCase):
            @classmethod
            def tearDownClass(cls):
                raise RuntimeError('DOC-CI-004 controlled class teardown error')
            def test_body(self):
                pass
        for case, count, holder in ((SetupError, 0, 'setUpClass'), (TeardownError, 1, 'tearDownClass')):
            with self.subTest(case=case.__name__):
                result = unittest.TextTestRunner(stream=io.StringIO(), resultclass=ci.EvidenceResult).run(
                    unittest.defaultTestLoader.loadTestsFromTestCase(case))
                self.assertFalse(result.wasSuccessful())
                self.assertEqual(result.testsRun, count)
                self.assertEqual(len(result.errors), 1)
                errors = [event for event in result.events if event['event'] == 'error']
                self.assertEqual(len(errors), 1)
                self.assertIn(holder, errors[0]['id'])
                self.assertEqual(sum(event['event'] == 'success' for event in result.events), count)


class RepresentativeFeatureTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp = tempfile.TemporaryDirectory(prefix='pol002-surfaces-')
        cls.addClassCleanup(cls.temp.cleanup)
        cls.dir = Path(cls.temp.name)
        # A new settings namespace, not ~/.config/alire or implicit outer -s.
        # The tool binaries/cache may be shared; selection authority is local.
        cls.environment = e.qualified_alire_environment(
            cls.dir / 'alire-settings', cls.dir / 'alire-selection.log')
        cls.repo = cls.dir / 'candidate'
        epoch.export(ROOT, e.HISTORY, cls.repo)
        def git(*args):
            return subprocess.check_output(['git', '-C', str(cls.repo), *args], stderr=subprocess.STDOUT).decode().strip()
        cls.git = staticmethod(git)
        git('init', '-q'); git('config', 'user.name', 'POL002 Synthetic'); git('config', 'user.email', 'pol002@example.invalid')
        git('add', '.'); git('commit', '-qm', 'synthetic target')
        cls.target = git('rev-parse', 'HEAD')
        def append(name, text):
            p = cls.repo / name
            p.write_text(p.read_text() + text)
        # These are synthetic proof operations, not ReceiveEvent implementation
        # or fabricated provider success. Actual arithmetic is executed below.
        append('native/include/ams_mel/abi.h', '\ntypedef uint32_t ams_mel_rf_qualification_observation;\n')
        append('native/src/rf_product_rx.cpp', '\nstatic_assert(sizeof(ams_mel_rf_qualification_observation) == sizeof(uint32_t));\n')
        append('native/tests/mock_rf_provider.cpp', '\nstatic_assert(sizeof(uint32_t) == 4, "synthetic observation fixture width");\n')
        append('rust/ams-mel-sys/src/lib.rs', '\npub type AmsMelRfQualificationObservation = u32;\n')
        append('python/ams_mel/_native.py', '\n_RF_QUALIFICATION_OBSERVATION = ctypes.c_uint32\n')
        ads = cls.repo / 'ada/src/ams-mel-rf-product_rx.ads'
        ads.write_text(ads.read_text().replace('\nprivate\n', '''
   function Qualification_Observation (Value : Natural) return Natural;
   --  Preserve a synthetic observation count as an independent value.
   --  @param Value Exact observation count supplied by the caller.
   --  @return The unchanged observation count.

private
'''))
        adb = cls.repo / 'ada/src/ams-mel-rf-product_rx.adb'
        text = adb.read_text()
        marker = '\nbegin\n   if Long_Float'
        if text.count(marker) != 1:
            raise AssertionError('synthetic fixture package elaboration boundary changed')
        adb.write_text(text.replace(marker, '\n   function Qualification_Observation (Value : Natural) return Natural is\n   begin\n      return Value;\n   end Qualification_Observation;\n' + marker))
        probe = cls.repo / 'native/tests/product_rx_v10_layout_probe.c'
        probe.write_text('#include <ams_mel/abi.h>\nint main(void) { ams_mel_rf_qualification_observation v = UINT32_MAX; return v == UINT32_MAX ? 0 : 1; }\n')
        cls.value_before_format = None
        cls.command(['make', 'format-ada'], 'format')
        git('add', '.'); git('commit', '-qm', 'synthetic ABI native Ada mock and raw surface changes')
        cls.head = git('rev-parse', 'HEAD')
        cls.value = s.qualify(cls.repo, cls.target, cls.head)
        original_classification = copy.deepcopy(cls.value)
        cls.output = cls.dir / 'epoch'
        cls.qualified = epoch.qualify_evidence(cls.repo, cls.value, cls.output, Path(os.environ['GNATDOC_PREFIX']))
        if cls.value != original_classification:
            raise AssertionError('candidate generation mutated independently bound source classification')
        cls.command(['gcc', '-std=c11', '-Wall', '-Wextra', '-Wpedantic', '-Werror', '-I', 'native/include',
                 'native/tests/product_rx_v10_layout_probe.c', '-o', str(cls.dir / 'probe')], 'c11')
        cls.command([str(cls.dir / 'probe')], 'c11-run')
        cls.command(['make', 'test-native'], 'native')
        cls.command(['make', 'test-ada'], 'ada')
        cls.command(['make', 'check-ada-format'], 'format-check')
        # Independently execute the new Ada implementation, rather than merely
        # observing that semantic/documentation generation accepted its profile.
        consumer = cls.dir / 'consumer'
        consumer.mkdir()
        (consumer / 'proof.adb').write_text('''with AMS.MEL.RF.Product_Rx;
procedure Proof is
begin
   if AMS.MEL.RF.Product_Rx.Qualification_Observation (Natural'Last) /= Natural'Last
     or else AMS.MEL.RF.Product_Rx.Qualification_Observation (0) /= 0
   then
      raise Program_Error with "synthetic observation fidelity failed";
   end if;
end Proof;
''')
        (consumer / 'proof.gpr').write_text('with "' + str(cls.repo / 'ada/ams_mel.gpr') + '";\n'
            'project Proof is\n for Main use ("proof.adb");\n for Object_Dir use "obj";\n'
            ' for Exec_Dir use "bin";\n package Compiler is\n for Default_Switches ("Ada") use '
            '("-gnat2022", "-gnata", "-gnatVa", "-gnatwa", "-gnatwe");\n end Compiler;\nend Proof;\n')
        with mock.patch.dict(os.environ, {'GPR_PROJECT_PATH': str(cls.repo / 'native')}):
            cls.command(['gprbuild', '-p', '-P', str(consumer / 'proof.gpr')], 'ada-proof-build')
        with mock.patch.dict(os.environ, {'LD_LIBRARY_PATH': str(cls.repo / 'native/build/lib')}):
            cls.command([str(consumer / 'bin/proof')], 'ada-proof-run')
        cls.command(['cargo', 'check', '--manifest-path', 'rust/Cargo.toml', '--workspace'], 'rust')
        cls.command(['python3', '-W', 'error', '-m', 'compileall', '-q', 'python/ams_mel'], 'python')

    @classmethod
    def command(cls, command, name):
        env = {**cls.environment}
        # GPR/runtime overrides in the focused consumer are intentionally scoped.
        for key in ('GPR_PROJECT_PATH', 'LD_LIBRARY_PATH'):
            if key in os.environ:
                env[key] = os.environ[key]
        with (cls.dir / (name + '.log')).open('wb') as stream:
            result = subprocess.run(command, cwd=cls.repo, env=env, stdin=subprocess.DEVNULL,
                                    stdout=stream, stderr=subprocess.STDOUT)
        if result.returncode:
            raise AssertionError('representative command failed: ' + ' '.join(command) + '\n' + (cls.dir / (name + '.log')).read_text()[-5000:])

    def test_representative_surfaces_are_actual_candidate_delta(self):
        paths = {r['path'] for r in self.value['delta']}
        self.assertLessEqual({'native/include/ams_mel/abi.h', 'native/src/rf_product_rx.cpp',
                             'native/tests/mock_rf_provider.cpp', 'ada/src/ams-mel-rf-product_rx.ads',
                             'ada/src/ams-mel-rf-product_rx.adb', 'rust/ams-mel-sys/src/lib.rs',
                             'python/ams_mel/_native.py', 'native/tests/product_rx_v10_layout_probe.c'}, paths)
        self.assertFalse(self.value['independent_approval'])

    def test_fresh_nested_formatter_is_noninteractive_and_task_scoped(self):
        settings = Path(self.environment['ALIRE_SETTINGS_DIR'])
        self.assertTrue(settings.is_relative_to(self.dir))
        self.assertNotEqual(settings, Path.home() / '.config/alire')
        log = (self.dir / 'format.log').read_text()
        self.assertNotIn('Press Enter', log)
        self.assertNotIn('selected automatically this toolchain', log)
        self.assertNotIn('ERROR:', log)
        self.assertTrue((self.repo / 'ada/tests/alire').is_dir())
        for name, expected in (('gnat', 'gnat_native=16.1.0'), ('gprbuild', 'gprbuild=26.0.1')):
            selected = subprocess.check_output(['alr', '-n', 'settings', '--global', '--get',
                                                'toolchain.use.' + name], cwd=self.dir,
                                               env=self.environment, stdin=subprocess.DEVNULL, text=True)
            self.assertEqual(selected.strip(), expected)

    def test_representative_candidate_publication_qualifies(self):
        self.assertGreater(self.qualified['added_declarations'], 0)
        self.assertEqual(self.qualified['candidate_commit'], self.head)
        epoch.verify(self.repo, self.output, self.target, self.head)

    def test_historical_artifact_cannot_replace_candidate(self):
        with self.assertRaises(ValueError): epoch.verify(self.repo, self.output, self.target, self.target)

    def test_another_candidate_commit_cannot_reuse_artifact(self):
        with mock.patch.object(s, 'qualify', return_value={**self.value, 'candidate_commit': 'f' * 40}):
            with self.assertRaises(ValueError): epoch.verify(self.repo, self.output, self.target, self.head)

    def test_policy_and_production_mixed_delta_rejected(self):
        with tempfile.TemporaryDirectory() as t:
            copy = Path(t) / 'source'; __import__('shutil').copytree(self.repo, copy)
            p = copy / 'docs/tools/feature_source.py'; p.parent.mkdir(parents=True, exist_ok=True); p.write_text('allow_everything = True\n')
            subprocess.run(['git', '-C', str(copy), 'add', '.'], check=True)
            subprocess.run(['git', '-C', str(copy), 'commit', '-qm', 'mixed checker'], check=True)
            with self.assertRaises(ValueError): e.classify(copy, self.target, s.revision(copy, 'HEAD'))


class EventAuthorityTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='pol002-event-')
        self.addCleanup(self.temp.cleanup)
        self.directory = Path(self.temp.name)
        self.repo = self.directory / 'source'
        subprocess.run(['git', 'clone', '--shared', '--no-checkout', str(ROOT), str(self.repo)],
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

    def test_policy_change_without_production_is_separate_review_case(self):
        (self.repo / 'docs/tools/epoch_ci.py').write_text('synthetic proposed policy\n')
        self.git('add', '.'); self.git('commit', '-qm', 'policy-only proposal')
        self.assertEqual(e.classify(self.repo, e.HISTORY, self.git('rev-parse', 'HEAD'))['case'], 'policy-only')

    def test_policy_mixed_with_vendor_is_rejected(self):
        (self.repo / 'docs/tools/epoch_ci.py').write_text('synthetic proposed policy\n')
        path = next((self.repo / 'native/vendor').rglob('*.h'))
        path.write_bytes(path.read_bytes() + b'\n/* unauthorized */\n')
        self.git('add', '.'); self.git('commit', '-qm', 'mixed policy vendor')
        with self.assertRaises(ValueError): e.classify(self.repo, e.HISTORY, self.git('rev-parse', 'HEAD'))


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
        self.assertIn('  build:\n    if: always()\n    needs: [prepare-documentation, enhanced-tests]',text)
        self.assertIn('shard: [navigation, publication, live, symbols-catalog, types, remaining]',text)
        self.assertIn("if: github.ref == 'refs/heads/main' && github.event_name != 'pull_request'",text)
        prepare=text.split('\n  enhanced-tests:\n')[0]
        self.assertLess(prepare.index('epoch_ci.py select'),prepare.index('Establish historical and candidate epochs'))
        self.assertLess(prepare.index('Establish historical and candidate epochs'),prepare.index('Upload same-run qualified inputs'))
        deploy=text.split('\n  deploy:\n')[1].split('\n  verify-live:\n')[0]
        self.assertLess(deploy.index("' deployment --root"),deploy.index('actions/upload-pages-artifact'))
        self.assertIn('path: .doc-tools/epochs/candidate/published/',deploy)

    def test_generation_does_not_mutate_source_classification(self):
        evidence={'target_commit':'a'*40,'candidate_commit':'b'*40,'case':'feature-source'}
        original=copy.deepcopy(evidence)
        # Reach a controlled stop after generation's copy boundary. It must leave
        # even nested caller-owned source identity objects untouched.
        with tempfile.TemporaryDirectory() as t, mock.patch.object(epoch,'readonly_qualification_inputs',side_effect=ValueError('stop')):
            with self.assertRaises(ValueError):
                epoch.qualify_evidence(ROOT,evidence,Path(t)/'output',Path(t)/'tools')
        self.assertEqual(evidence,original)


class PublicationApprovalTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp = tempfile.TemporaryDirectory(prefix='pol002-publication-approval-')
        cls.addClassCleanup(cls.temp.cleanup)
        cls.dir = Path(cls.temp.name)
        cls.repo = cls.dir / 'source'
        e.historical_checkout(cls.repo)
        def git(*args):
            return subprocess.check_output(['git', '-C', str(cls.repo), *args], stderr=subprocess.STDOUT).decode().strip()
        cls.git = staticmethod(git)
        git('config', 'user.name', 'POL002 Approval Fixture')
        git('config', 'user.email', 'pol002@example.invalid')
        cls.output = epoch.qualify(cls.repo, e.HISTORY, e.HISTORY, cls.dir / 'historical',
                                   Path(os.environ['GNATDOC_PREFIX']))
        cls.pristine = cls.output['pristine']
        cls.published = cls.output['publication']['files']
        (cls.repo / 'docs/historical-policy-fixture.md').write_text('Historical policy-only publication.\n')
        git('add', '.'); git('commit', '-qm', 'synthetic historical policy merge')
        cls.historical_policy = git('rev-parse', 'HEAD')
        # A feature head and distinct actual integration commit, not aliases.
        p = cls.repo / 'native/src/rf_product_rx.cpp'
        p.write_text(p.read_text() + '\n/* synthetic approval identity observation */\n')
        git('add', '.'); git('commit', '-qm', 'synthetic feature PR head')
        cls.pr_head = git('rev-parse', 'HEAD')
        tree = git('rev-parse', 'HEAD^{tree}')
        cls.merge = git('commit-tree', tree, '-p', e.HISTORY, '-p', cls.pr_head, '-m', 'synthetic post-merge identity')
        git('checkout', '--detach', cls.merge)
        cls.entry = {'commit': cls.merge, 'tree': tree,
                     'production_sha256': epoch.production_fingerprint(cls.repo, cls.merge),
                     'pristine_sha256': __import__('build_enhanced_site').fingerprint(cls.pristine),
                     'published_sha256': __import__('build_enhanced_site').fingerprint(cls.published)}
        path = cls.repo / 'docs/tools/source_epoch.py'
        path.write_text('REVIEWED_EPOCHS = ' + repr((cls.entry,)) + '\n')
        git('add', '.'); git('commit', '-qm', 'independently reviewed synthetic publication approval')
        cls.approval_target = git('rev-parse', 'HEAD')
        (cls.repo / 'docs/approved-fixture.md').write_text('Policy-only descendant.\n')
        git('add', '.'); git('commit', '-qm', 'publication candidate after approval')
        cls.candidate = git('rev-parse', 'HEAD')

    def authorize(self, candidate, target):
        return epoch.publication_authority(self.repo, candidate, target=target,
                                           pristine=self.pristine, published=self.published)

    def test_unchanged_historical_publication_needs_no_feature_registry(self):
        value = self.authorize(e.HISTORY, e.HISTORY)
        self.assertEqual(value['kind'], 'historical-preservation')

    def test_historical_policy_only_merge_keeps_automatic_publication(self):
        # Production bytes and whole historical outputs, not the policy case label.
        value = self.authorize(self.historical_policy, e.HISTORY)
        self.assertEqual(value['reviewed_commit'], e.HISTORY)
        self.assertEqual(value['commit'], self.historical_policy)

    def test_reviewed_postmerge_feature_publication_passes(self):
        value = self.authorize(self.candidate, self.approval_target)
        self.assertEqual(value['kind'], 'reviewed-feature-epoch')
        self.assertEqual(value['reviewed_commit'], self.merge)
        self.assertEqual(value['commit'], self.candidate)

    def test_unreviewed_feature_main_push_cannot_publish(self):
        with self.assertRaisesRegex(ValueError, 'no independently reviewed'):
            self.authorize(self.merge, e.HISTORY)

    def test_hosted_main_mechanical_result_is_not_publication_permission(self):
        record = {'identity': {'event': 'push', 'qualification_mode': 'hosted',
                               'source_sha': self.merge, 'target_sha': e.HISTORY},
                  'candidate': {'pristine': self.pristine, 'publication': {'files': self.published}}}
        with mock.patch.object(e, 'ROOT', self.repo):
            self.assertIsNone(e.publication_decision(record))
        record['identity'].update(source_sha=self.candidate, target_sha=self.approval_target)
        with mock.patch.object(e, 'ROOT', self.repo):
            self.assertEqual(e.publication_decision(record)['reviewed_commit'], self.merge)

    def test_same_tree_pr_head_is_not_postmerge_identity(self):
        self.assertNotEqual(self.pr_head, self.merge)
        self.assertEqual(self.git('rev-parse', self.pr_head + '^{tree}'), self.entry['tree'])
        with self.assertRaises(ValueError): self.authorize(self.pr_head, self.approval_target)

    def test_untrusted_candidate_registry_cannot_self_approve(self):
        with self.assertRaises(ValueError): self.authorize(self.approval_target, e.HISTORY)

    def test_manifest_approval_claim_does_not_grant_authority(self):
        record = {'identity': {'event': 'push', 'qualification_mode': 'hosted', 'source_sha': self.merge,
                               'target_sha': e.HISTORY},
                  'candidate': {'pristine': self.pristine, 'publication': {'files': self.published},
                                'independent_approval': True, 'publication_epoch_approved': True},
                  'publication_authority': self.entry, 'deployable': True}
        with mock.patch.object(e, 'ROOT', self.repo):
            self.assertIsNone(e.publication_decision(record))

    def test_mismatched_merge_tree_is_rejected(self):
        wrong = {**self.entry, 'tree': '0' * 40}
        with mock.patch.object(epoch, 'trusted_publication_epochs', return_value=(wrong,)):
            with self.assertRaisesRegex(ValueError, 'post-merge identity'):
                self.authorize(self.candidate, self.approval_target)

    def test_changed_artifact_cannot_reuse_reviewed_source_approval(self):
        with self.assertRaises(ValueError):
            epoch.publication_authority(self.repo, self.candidate, target=self.approval_target,
                                        pristine=self.pristine, published={**self.published, 'fake': '0' * 64})

    def test_deployment_rechecks_authority_despite_forged_passing_gate(self):
        identity = {'event': 'push', 'qualification_mode': 'hosted', 'source_sha': self.merge, 'target_sha': e.HISTORY}
        record = {'identity': identity, 'candidate': {'pristine': self.pristine,
                   'publication': {'files': self.published}}}
        qualified = {'artifact_id': '1', 'bundle_sha256': 'a' * 64, 'manifest_sha256': 'b' * 64}
        gate = {'mechanically_qualified': True, 'deployable': True, 'identity': identity,
                'epochs_sha256': e.digest(record), 'qualified': qualified,
                'report_sha256': {k: 'c' * 64 for k in ci.SHARDS}, 'publication_authority': self.entry}
        args = argparse.Namespace(directory='unused', gate='unused', gate_sha='d' * 64,
                                  artifact_id='1', bundle_sha='a' * 64, manifest_sha='b' * 64)
        with mock.patch.object(e, 'ROOT', self.repo), mock.patch.object(e, 'verify', return_value=record), \
             mock.patch.object(e, 'context', return_value=identity), mock.patch.object(ci, 'file_sha', return_value='d' * 64), \
             mock.patch.object(ci, 'load', return_value=gate), mock.patch.dict(os.environ, {
                 'GITHUB_EVENT_NAME': 'push', 'GITHUB_REF': 'refs/heads/main'}):
            with self.assertRaisesRegex(ValueError, 'no independently reviewed publication authority'):
                e.deployment(args)


class AdditiveDocumentationTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp = tempfile.TemporaryDirectory(prefix='pol002-doc-evolution-')
        cls.addClassCleanup(cls.temp.cleanup)
        cls.dir = Path(cls.temp.name)
        cls.repo = cls.dir / 'source'
        epoch.export(ROOT, e.HISTORY, cls.repo)
        def git(*args):
            return subprocess.check_output(['git','-C',str(cls.repo),*args],stderr=subprocess.STDOUT).decode().strip()
        cls.git=staticmethod(git)
        git('init','-q');git('config','user.name','POL002 Documentation');git('config','user.email','pol002@example.invalid')
        git('add','.');git('commit','-qm','synthetic target');cls.base=git('rev-parse','HEAD')
        path=cls.repo/'ada/src/ams-mel-rf-product_rx.ads'
        text=path.read_text();needle='   --  Count copied association IDs, not ReceiveEvent objects.'
        if text.count(needle)!=1:raise AssertionError('synthetic source prose boundary changed')
        path.write_text(text.replace(needle,'   --  Read copied association IDs, not ReceiveEvent objects.'))
        git('add','.');git('commit','-qm','source-backed existing declaration prose change')
        cls.head=git('rev-parse','HEAD')
        cls.result=epoch.qualify(cls.repo,cls.base,cls.head,cls.dir/'epoch',Path(os.environ['GNATDOC_PREFIX']))

    def test_intentionally_edited_source_prose_can_qualify(self):
        self.assertEqual(self.result['added_declarations'],0)
        self.assertEqual(self.result['candidate_commit'],self.head)

    def test_unchanged_anchors_profiles_preserved(self):
        old=json.loads((ROOT/'docs/gnatdoc-native-anchors.json').read_text())
        self.assertEqual(old,self.result['native'])

    def test_no_coverage_or_warning_baseline_expansion(self):
        self.assertEqual(self.result['documented_elements'],4294)
        self.assertEqual(self.result['total_elements'],4295)
        self.assertFalse(self.result['independent_approval'])

    def test_unchanged_source_cannot_authorize_mutated_native_section(self):
        from check_doc_coverage import measure
        from unittest import mock
        import shutil
        left=self.dir/'left';right=self.dir/'right'
        epoch.export(self.repo,self.head,left);epoch.export(self.repo,self.head,right)
        for root in (left,right): shutil.copytree(self.dir/'epoch/pristine',root/'_site')
        report=measure(left/'_site',left)
        entry=next(iter(report['native'].values()))
        path=right/'_site/api'/entry['page']
        marker='<h4 id='+entry['anchor']+'>'
        text=path.read_text();path.write_text(text.replace(marker,marker+'fabricated ',1))
        with self.assertRaisesRegex(ValueError,'complete historical declaration'):
            epoch.inherited_declarations(left/'_site',report,right/'_site',report)


class CI001R3Tests(unittest.TestCase):
    """Independent committed source transitions and workflow mutation controls."""

    def setUp(self):
        import shutil
        import ci001r3_source as correction
        self.correction = correction
        self.temp = tempfile.TemporaryDirectory(prefix='ci001r3-git-')
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.git('init', '-q')
        self.git('config', 'user.name', 'CI001R3 Fixture')
        self.git('config', 'user.email', 'ci001r3@example.invalid')
        self.names = [*correction.TESTS, correction.QUALIFIER, correction.VERIFIER,
                     'docs/tools/ci001r3_source.py', 'docs/tools/epoch_ci.py',
                     'docs/tests/pol002_epoch_tests.py']
        for name in self.names:
            if name not in s.tree(ROOT, correction.BASE):
                continue
            data = s.blob(ROOT, correction.BASE, name)
            self.write(name, data)
        self.write('.github/workflows/documentation.yml',
                   (ROOT / '.github/workflows/documentation.yml').read_bytes())
        self.write('docs/tools/source_epoch.py', (ROOT / 'docs/tools/source_epoch.py').read_bytes())
        self.write('native/src/ir_stream.cpp', b'original\n')
        self.base = self.commit()
        objects = s.git(ROOT, 'rev-parse', '--path-format=absolute', '--git-path', 'objects').decode().strip()
        (self.root / '.git/objects/info/alternates').write_text(objects + '\n')

    def git(self, *args):
        return subprocess.check_output(['git', '-C', str(self.root), *args], stderr=subprocess.STDOUT).decode().strip()

    def write(self, name, data):
        path = self.root / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)

    def commit(self):
        self.git('add', '.'); self.git('commit', '-qm', 'committed CI001R3 fixture')
        return self.git('rev-parse', 'HEAD')

    def candidate(self):
        for name in self.names:
            # These are historical correction controls, not an authorization
            # shortcut for today's integrated successor. Keep the exact old
            # verifier/checker fixture independently pinned to its Git source.
            data = (s.blob(ROOT, 'e7eaba2cdf61a51f51f5a12d27e246c1041a1ce4', name)
                    if name in (self.correction.VERIFIER, 'docs/tools/ci001r3_source.py')
                    else (ROOT / name).read_bytes())
            self.write(name, data)
        return self.commit()

    def classify(self):
        return e.classify(self.root, self.base, self.git('rev-parse', 'HEAD'))

    def test_exact_correction_is_policy_only_not_approval(self):
        self.candidate()
        value = self.classify()
        self.assertEqual(value['case'], 'policy-only')
        self.assertFalse(value['independent_approval'])
        self.assertFalse(value['publication_epoch_approved'])
        self.assertTrue(set(self.correction.TESTS).isdisjoint(e.POLICY_PATHS))
        self.assertNotIn(self.correction.QUALIFIER, e.POLICY_PATHS)

    def test_exact_inherited_views_restore_all_unaffected_bytes(self):
        self.candidate()
        restored = self.correction.inherited_sources(self.root, lambda rev, name: s.blob(ROOT, rev, name))
        self.assertEqual(set(restored), set(self.correction.TESTS))
        for name in restored:
            self.assertEqual(restored[name], s.blob(ROOT, self.correction.BASE, name))

    def test_historical_fixtures_continue_to_original_checks(self):
        for revision in (ci.CI001_SHA, s.HISTORICAL_MAIN):
            for name in self.correction.TESTS:
                self.write(name, s.blob(ROOT, revision, name))
            self.assertEqual(self.correction.inherited_sources(
                self.root, lambda rev, name: s.blob(ROOT, rev, name)), {})

    def test_unchanged_ci001r2_minimal_fixture_restores_current_control(self):
        self.write('docs/tests/test_live_docs_site.py', s.blob(ROOT, ci.CI001_SHA, 'docs/tests/test_live_docs_site.py'))
        name = 'docs/tests/test_documentation_ci.py'
        self.write(name, (ROOT / name).read_bytes())
        (self.root / self.correction.QUALIFIER).unlink()
        views = self.correction.inherited_sources(self.root, lambda rev, path: s.blob(ROOT, rev, path))
        self.assertEqual(views, {name: s.blob(ROOT, self.correction.BASE, name)})

    def test_incomplete_transition_rejects(self):
        for name in self.correction.TESTS:
            self.git('reset', '--hard', self.base)
            self.candidate()
            self.write(name, s.blob(ROOT, self.correction.BASE, name))
            self.commit()
            with self.assertRaises(ValueError): self.classify()

    def test_unknown_method_helper_test_name_or_qualifier_rejects(self):
        controls = [
            ('docs/tests/test_live_docs_site.py', b'live_contract(ROOT)', b'pass'),
            ('docs/tests/test_live_docs_site.py', b'test_pr_fixture_discovery_retained', b'test_renamed'),
            ('docs/tests/test_documentation_ci.py', b'deploy_contract(ci.ROOT)', b'pass'),
            ('docs/tools/documentation_ci.py', b'original 51 source-integrity assertions changed', b'deleted assertion'),
            ('docs/tools/ci001r3_workflow.py', b'== expected', b'!= expected'),
        ]
        for name, old, new in controls:
            with self.subTest(path=name, mutation=old):
                self.git('reset', '--hard', self.base)
                self.candidate()
                self.write(name, (self.root / name).read_bytes().replace(old, new))
                self.commit()
                with self.assertRaises(ValueError): self.classify()
        for extra in (b'\ndef unrelated_helper(): pass\n', b'\nclass Extra:\n    def test_extra(self): pass\n'):
            self.git('reset', '--hard', self.base); self.candidate()
            name = 'docs/tests/test_live_docs_site.py'
            self.write(name, (self.root / name).read_bytes() + extra); self.commit()
            with self.assertRaises(ValueError): self.classify()

    def test_stale_predecessor_rejects(self):
        self.write('docs/tests/test_live_docs_site.py', b'# stale source\n')
        self.base = self.commit(); self.candidate()
        with self.assertRaises(ValueError): self.classify()

    def test_unrelated_executable_registry_or_self_approval_rejects(self):
        for name, value in [('native/src/ir_stream.cpp', b'changed production\n'),
                            ('docs/tools/source_epoch.py', b'REVIEWED_EPOCHS = ("self-approved",)\n'),
                            ('docs/approval.json', b'{"independent_approval":true}\n'),
                            ('docs/tools/ci001r3_source.py', b'approve_everything = True\n')]:
            self.git('reset', '--hard', self.base); self.candidate()
            self.write(name, value); self.commit()
            with self.assertRaises(ValueError): self.classify()

    def test_workflow_mutations_reject_without_changing_protected_source(self):
        from ci001r3_workflow import live_contract, preparation_contract, deploy_contract
        original = (self.root / '.github/workflows/documentation.yml').read_bytes()
        controls = [
            (b'deployment --root', b'echo --root', deploy_contract),
            (b'docs/tools/epoch_ci.py select', b'docs/tools/epoch_ci.py bypass', live_contract),
            (b'--gate-sha', b'--unchecked-gate', deploy_contract),
            (b'publication, live, symbols-catalog', b'publication, symbols-catalog', preparation_contract),
            (b'fail-fast: false', b'fail-fast: true', preparation_contract),
            (b'artifact-ids: ${{ needs.build.outputs.gate_id }}', b'artifact-ids: 123', live_contract),
            (b"if: github.ref == 'refs/heads/main' && github.event_name != 'pull_request'", b'if: always()', deploy_contract),
            (b'EXPECTED_SHA: ${{ github.sha }}', b'EXPECTED_SHA: fake', live_contract),
            (b'      actions: read', b'      actions: write', live_contract),
            (b'--attempts 10 --interval 30', b'--attempts 999 --interval 0', live_contract),
            (b'      fail-fast: false', b'      fail-fast: false\n    if: false', preparation_contract),
        ]
        self.write('docs/tools/epoch_ci.py', (ROOT / 'docs/tools/epoch_ci.py').read_bytes())
        for old, new, verifier in controls:
            with self.subTest(mutation=old):
                self.assertIn(old, original)
                self.write('.github/workflows/documentation.yml', original.replace(old, new))
                with self.assertRaises((AssertionError, ValueError)): verifier(self.root)
        self.write('.github/workflows/documentation.yml', original)
        for verifier in (live_contract, preparation_contract, deploy_contract): verifier(self.root)

    def test_removed_source_integrity_inventory_or_canonical_execution_rejects(self):
        from ci001r3_workflow import preparation_contract
        original = (ROOT / 'docs/tools/epoch_ci.py').read_bytes()
        for old in (b'scripts/check_doc_source_integrity.py', b'docs/tools/qualify_examples.sh',
                    b"'inventory'", b"'--owner', 'scripts'", b"'seal', '--output'"):
            self.write('docs/tools/epoch_ci.py', original.replace(old, b'removed'))
            with self.assertRaises(AssertionError): preparation_contract(self.root)


class CI001R3OrderingTests(unittest.TestCase):
    """R1: precedence is not a missing-source exemption, using committed trees."""

    setUp = CI001R3Tests.setUp
    git = CI001R3Tests.git
    write = CI001R3Tests.write
    commit = CI001R3Tests.commit
    candidate = CI001R3Tests.candidate
    classify = CI001R3Tests.classify

    def complete_ci002(self):
        self.candidate()
        name = 'docs/tests/test_bootstrap_ci_policy.py'
        self.write(name, (ROOT / name).read_bytes())
        self.commit()
        identities = list(ci.source_methods((self.root / name).read_bytes(), 'test_bootstrap_ci_policy'))
        self.assertEqual(len(identities), 71)
        return name, {'scripts': [], 'enhanced': identities}

    def test_valid_ci002_does_not_excuse_missing_protected_sources(self):
        for name in self.correction.TESTS:
            with self.subTest(path=name):
                self.git('reset', '--hard', self.base)
                _, groups = self.complete_ci002()
                (self.root / name).unlink()
                self.commit()
                with self.assertRaises(FileNotFoundError):
                    ci.validate_inventory(groups, self.root)
                with self.assertRaises(ValueError): self.classify()

    def test_complete_ci002_changed_live_or_control_source_rejects(self):
        for name in self.correction.TESTS:
            self.git('reset', '--hard', self.base)
            self.complete_ci002()
            self.write(name, (self.root / name).read_bytes() + b'\n# unknown source\n')
            self.commit()
            with self.assertRaises(ValueError):
                self.correction.inherited_sources(self.root, lambda rev, path: s.blob(ROOT, rev, path))
            with self.assertRaises(ValueError): self.classify()

    def test_ci002_rejection_precedes_missing_sources(self):
        name, groups = self.complete_ci002()
        for protected in self.correction.TESTS:
            (self.root / protected).unlink()
        original = (self.root / name).read_bytes()
        self.write(name, original + b'\n# mutation\n'); self.commit()
        with self.assertRaisesRegex(ValueError, 'control source changed'):
            ci.validate_inventory(groups, self.root)
        self.write(name, original); self.commit()
        for ids in (groups['enhanced'][:-1], groups['enhanced'] + groups['enhanced'][:1]):
            with self.assertRaisesRegex(ValueError, 'missing or duplicated'):
                ci.validate_inventory({'scripts': [], 'enhanced': ids}, self.root)
        (self.root / name).unlink(); self.commit()
        with mock.patch.object(ci, 'ROOT', self.root):
            with self.assertRaisesRegex(ValueError, 'control source missing'):
                ci.validate_inventory({'scripts': [], 'enhanced': []})

    def test_unknown_and_duplicated_current_inventory_reject(self):
        inventory = ci.inventory()
        for mutation in ('unknown', 'duplicate'):
            groups = copy.deepcopy(inventory['groups'])
            groups['enhanced'].append('unknown.Test.test_fake' if mutation == 'unknown' else groups['enhanced'][0])
            with self.assertRaises(ValueError): ci.validate_inventory(groups)

    def test_dirty_or_staged_source_cannot_claim_exact_transition(self):
        self.candidate()
        name = 'docs/tests/test_live_docs_site.py'
        path = self.root / name
        path.write_bytes(path.read_bytes() + b'\n# dirty\n')
        with self.assertRaises(ValueError): self.classify()
        self.git('add', name)
        with self.assertRaises(ValueError): self.classify()


class CI001R3ActionsIntegrationTests(unittest.TestCase):
    """Exact merged Node 24 dependencies plus both committed policy routes."""

    setUp = CI001R3Tests.setUp
    git = CI001R3Tests.git
    write = CI001R3Tests.write
    commit = CI001R3Tests.commit
    candidate = CI001R3Tests.candidate
    classify = CI001R3Tests.classify

    def verify(self):
        from ci001r3_workflow import live_contract, preparation_contract, deploy_contract
        for verifier in (live_contract, preparation_contract, deploy_contract):
            verifier(self.root)

    def test_exact_reviewed_node24_actions_and_comments(self):
        from ci001r3_workflow import ACTION_PINS
        self.assertEqual(ACTION_PINS, {'actions/' + n: sha + ' # ' + release
                                      for n, (sha, release, _) in dependencies.PINS.items()})
        self.candidate()
        self.verify()

    def test_every_pin_tag_owner_comment_and_missing_action_rejects(self):
        self.candidate()
        name = '.github/workflows/documentation.yml'
        original = (self.root / name).read_text()
        for action, (sha, release, old) in dependencies.PINS.items():
            token = 'actions/' + action + '@' + sha + ' # ' + release
            for replacement in ('actions/' + action + '@' + '0' * 40 + ' # ' + release,
                                'actions/' + action + '@' + old,
                                'unreviewed/' + action + '@' + sha + ' # ' + release,
                                'actions/' + action + '@' + sha + ' # wrong-release', ''):
                with self.subTest(action=action, replacement=replacement):
                    self.write(name, original.replace(token, replacement).encode())
                    with self.assertRaises(AssertionError): self.verify()
        self.write(name, original.encode())
        self.verify()

    def test_pages_hidden_files_and_existing_security_contracts_reject(self):
        self.candidate()
        name = '.github/workflows/documentation.yml'
        original = (self.root / name).read_text()
        for old, new in [('          include-hidden-files: true\n', '          include-hidden-files: false\n'),
                         ('          include-hidden-files: true\n', ''),
                         ('  contents: read', '  contents: write'),
                         ('docs/tools/epoch_ci.py select', 'docs/tools/epoch_ci.py untrusted'),
                         ('artifact-ids:', 'unbound-artifact:'),
                         ("' deployment --root", "' bypass --root"),
                         ("' aggregate --root", "' bypass --root")]:
            with self.subTest(mutation=old):
                self.assertIn(old, original)
                self.write(name, original.replace(old, new).encode())
                with self.assertRaises(AssertionError): self.verify()
        self.write(name, original.encode())

    def test_reviewed_pin_cannot_replace_another_required_action_role(self):
        from ci001r3_workflow import ACTION_PINS
        self.candidate()
        name = '.github/workflows/documentation.yml'
        original = (self.root / name).read_text()
        import re
        references = list(re.finditer(r'uses: (actions/[^\n]+)', original))
        for reference in references:
            token = reference.group(1)
            replacement = ('actions/upload-artifact' if token.startswith('actions/checkout@')
                           else 'actions/checkout')
            changed = (original[:reference.start(1)] + replacement + '@' + ACTION_PINS[replacement]
                       + original[reference.end(1):])
            with self.subTest(action=token, position=reference.start(1)):
                self.write(name, changed.encode())
                with self.assertRaisesRegex(AssertionError, 'action role'): self.verify()
        self.write(name, original.encode())
        self.verify()

    def test_committed_action_transition_remains_atomic_and_not_approval(self):
        # No correction delta here: exercise CI-ACTIONS-001 independently with
        # real old/new Git blobs, not mocked policy decisions or manifests.
        self.git('reset', '--hard', self.base)
        for name, old, _ in e.ACTIONS_SOURCE:
            if old is not None: self.write(name, dependencies.original(name))
        target = self.commit()
        for name, _, _ in e.ACTIONS_SOURCE:
            self.write(name, s.blob(ROOT, dependencies.MIGRATION, name))
        self.write('docs/tools/epoch_ci.py', (ROOT / 'docs/tools/epoch_ci.py').read_bytes())
        head = self.commit()
        value = e.classify(self.root, target, head)
        self.assertEqual(value['case'], 'policy-only')
        self.assertFalse(value['independent_approval'])
        self.assertFalse(value['publication_epoch_approved'])
        for name, _, _ in e.ACTIONS_SOURCE:
            original = (self.root / name).read_bytes()
            self.write(name, original + b'\n# unreviewed\n'); self.commit()
            with self.assertRaises(ValueError): e.classify(self.root, target, self.git('rev-parse', 'HEAD'))
            self.git('reset', '--hard', head)
        for name, old, _ in e.ACTIONS_SOURCE:
            if old is None: (self.root / name).unlink()
            else: self.write(name, dependencies.original(name))
            self.commit()
            with self.assertRaises(ValueError): e.classify(self.root, target, self.git('rev-parse', 'HEAD'))
            self.git('reset', '--hard', head)

    def test_committed_correction_on_merged_main_rejects_mixed_and_registry_changes(self):
        self.candidate()
        head = self.git('rev-parse', 'HEAD')
        value = self.classify()
        self.assertEqual(value['case'], 'policy-only')
        self.assertFalse(value['independent_approval'])
        self.assertFalse(value['publication_epoch_approved'])
        for name in ['.github/workflows/documentation.yml', 'docs/tools/test_actions_dependencies.py',
                     'scripts/check_doc_source_integrity.py', 'docs/tools/feature_source.py']:
            self.write(name, (ROOT / name).read_bytes() + b'\n# candidate approval\n'); self.commit()
            with self.assertRaises(ValueError): self.classify()
            self.git('reset', '--hard', head)

    def test_real_merged_main_committed_correction_and_incomplete_successors(self):
        merged_main = '219b00c75210f5985d9889e8b541da2e7b658f04'
        self.git('read-tree', '--reset', '-u', merged_main)
        self.base = self.commit()
        self.candidate()
        head = self.git('rev-parse', 'HEAD')
        value = self.classify()
        self.assertEqual(value['case'], 'policy-only')
        self.assertFalse(value['independent_approval'])
        self.assertFalse(value['publication_epoch_approved'])
        for name in self.correction.TESTS:
            self.write(name, s.blob(ROOT, merged_main, name)); self.commit()
            with self.assertRaises(ValueError): self.classify()
            self.git('reset', '--hard', head)
        for name, _, _ in e.ACTIONS_SOURCE:
            self.write(name, (ROOT / name).read_bytes() + b'\n# unauthorized combination\n')
            self.commit()
            with self.assertRaises(ValueError): self.classify()
            self.git('reset', '--hard', head)


class DiagnosticIntegrationTests(unittest.TestCase):
    """Complete committed Git objects, never candidate-owned approval data."""

    def setUp(self):
        import doc_ci004_source as integration
        self.policy = integration
        self.temp = tempfile.TemporaryDirectory(prefix='doc-ci004-integration-')
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name) / 'source'
        subprocess.run(['git', 'clone', '--shared', '--no-checkout', str(ROOT), str(self.root)],
                       check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        self.git('checkout', '--detach', integration.PREDECESSOR)
        self.git('config', 'user.name', 'DOC-CI-004 integration control')
        self.git('config', 'user.email', 'doc-ci004@example.invalid')
        self.base = integration.PREDECESSOR
        for name in [*integration.SUCCESSOR, integration.POLICY]:
            self.write(name, (ROOT / name).read_bytes())
        self.head = self.commit()

    def git(self, *args):
        return subprocess.check_output(['git', '-C', str(self.root), *args],
                                       stderr=subprocess.STDOUT).decode().strip()

    def write(self, name, data):
        path = self.root / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)

    def commit(self):
        self.git('add', '.'); self.git('commit', '-qm', 'committed integrated control')
        return self.git('rev-parse', 'HEAD')

    def classify(self):
        return e.classify(self.root, self.base, self.git('rev-parse', 'HEAD'))

    def test_exact_successor_from_both_verified_predecessors_is_not_approval(self):
        for base in (self.policy.MAIN, self.policy.PREDECESSOR):
            self.base = base
            value = self.classify()
            self.assertEqual(value['case'], 'policy-only')
            self.assertFalse(value['independent_approval'])
            self.assertFalse(value['publication_epoch_approved'])

    def test_every_complete_successor_file_and_mode_is_required(self):
        for name in [*self.policy.SUCCESSOR, self.policy.POLICY]:
            for mutation in ('unknown', 'missing', 'executable'):
                with self.subTest(name=name, mutation=mutation):
                    path = self.root / name
                    if mutation == 'unknown': path.write_bytes(path.read_bytes() + b'\n# unreviewed\n')
                    elif mutation == 'missing': path.unlink()
                    else: path.chmod(0o755)
                    self.commit()
                    with self.assertRaises((ValueError, AssertionError)): self.classify()
                    self.git('reset', '--hard', self.head)

    def test_diagnostic_workflow_without_runner_or_policy_is_rejected(self):
        for name in ('docs/tools/epoch_ci.py', 'docs/tools/ci001r3_workflow.py',
                     'docs/tools/ci001r3_source.py', self.policy.POLICY):
            self.git('reset', '--hard', self.head)
            original = s.tree(ROOT, self.policy.PREDECESSOR)
            if name in original: self.write(name, s.blob(ROOT, self.policy.PREDECESSOR, name))
            else: (self.root / name).unlink()
            self.commit()
            with self.assertRaises(ValueError): self.classify()

    def test_unrelated_workflow_production_registry_and_policy_reject(self):
        for name in ('native/src/ir_stream.cpp', '.github/workflows/unreviewed.yml',
                     '.github/workflows/ci.yml', 'docs/tools/source_epoch.py',
                     'docs/tools/feature_source.py', 'scripts/extra_checker.py',
                     'docs/approval.json'):
            self.write(name, b'candidate-provided approval is not authority\n')
            self.commit()
            with self.assertRaises(ValueError): self.classify()
            self.git('reset', '--hard', self.head)

    def test_dirty_staged_committed_mismatch_reject(self):
        name = 'docs/tools/epoch_ci.py'
        self.write(name, (self.root / name).read_bytes() + b'\n# dirty\n')
        with self.assertRaises(ValueError): self.classify()
        self.git('add', name)
        with self.assertRaises(ValueError): self.classify()
        self.commit()
        with self.assertRaises(ValueError): self.classify()

    def test_later_successor_does_not_replay_old_atomic_action_migration(self):
        self.base = self.head
        self.write('docs/integration-followup.md', b'Documentation only; no approval.\n')
        self.commit()
        value = self.classify()
        self.assertEqual(value['case'], 'documentation-only')
        self.assertFalse(value['independent_approval'])
        self.assertFalse(value['publication_epoch_approved'])

    def test_actual_diagnostic_step_and_canonical_aggregate_mutations_reject(self):
        from ci001r3_workflow import preparation_contract, deploy_contract
        name = '.github/workflows/documentation.yml'
        original = (self.root / name).read_text()
        marker = '      - name: Upload separate non-authorizing failure diagnostics\n'
        before, tail = original.split(marker)
        diagnostic, after = tail.split('\n  # Keep the merge-required check name.', 1)
        variants = [before + marker + diagnostic.replace(a, b) + '\n  # Keep the merge-required check name.' + after
                    for a, b in [('if: always()', 'if: success()'),
                                 ('if: always()\n', ''),
                                 ('documentation-failure-diagnostics-', 'documentation-result-'),
                                 ('${{ github.run_attempt }}', '1'),
                                 ('${{ runner.temp }}/failure-diagnostics/${{ matrix.shard }}/',
                                  '${{ runner.temp }}/results/${{ matrix.shard }}.json'),
                                 ('if-no-files-found: warn', 'if-no-files-found: error'),
                                 ('# v6.0.0', '# unreviewed')]]
        variants += [original.replace('pattern: documentation-result-*', 'pattern: documentation-failure-diagnostics-*'),
                     original.replace('pattern: documentation-result-*', 'run-id: 12\n          pattern: documentation-result-*'),
                     original.replace('--diagnostics "$RUNNER_TEMP/failure-diagnostics/${{ matrix.shard }}"',
                                      '--diagnostics "$RUNNER_TEMP/results/${{ matrix.shard }}.json"')]
        for index, text in enumerate(variants):
            with self.subTest(mutation=index):
                self.write(name, text.encode())
                with self.assertRaises(AssertionError):
                    preparation_contract(self.root); deploy_contract(self.root)
                self.commit()
                with self.assertRaises((ValueError, AssertionError)): self.classify()
                self.git('reset', '--hard', self.head)

    def test_original_inventory_is_complete_and_unchanged(self):
        inventory = ci.inventory()
        self.assertEqual(len(inventory['groups']['scripts']), 142)
        self.assertEqual(len(inventory['groups']['enhanced']), 508)
        self.assertEqual(set(inventory['shards']), set(ci.SHARDS))


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
