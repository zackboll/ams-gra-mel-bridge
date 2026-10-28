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
        self.assertIn('uses: actions/upload-artifact@v4', upload)
        self.assertIn('name: documentation-failure-diagnostics-', upload)
        self.assertIn('path: ${{ runner.temp }}/failure-diagnostics/${{ matrix.shard }}/', upload)
        self.assertIn('pattern: documentation-result-*', workflow)
        self.assertNotIn('pattern: documentation-failure-diagnostics-', workflow)
        self.assertIn('needs: [prepare-documentation, build]', workflow)
        self.assertIn("all(j['result'] == 'success' for j in jobs.values())", workflow)


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
        for name in ('docs/tests/test_live_docs_site.py', 'docs/tests/test_documentation_ci.py',
                     'docs/tests/ci001-inherited-inventory.json'):
            self.assertEqual((ROOT / name).read_bytes(),
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
