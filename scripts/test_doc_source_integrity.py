"""Dependency integrity and security controls, not development source freezes."""

import hashlib
from pathlib import Path
import shutil
import tempfile
import unittest

from check_doc_source_integrity import ROOT, check, vendor_integrity, workflow_security


class IntegrityTests(unittest.TestCase):
    def setUp(self):
        temp = tempfile.TemporaryDirectory()
        self.addCleanup(temp.cleanup)
        self.root = Path(temp.name)
        shutil.copytree(ROOT / '.github', self.root / '.github')
        shutil.copytree(ROOT / 'docs/tools', self.root / 'docs/tools',
                        ignore=shutil.ignore_patterns('__pycache__'))
        path = self.root / 'native/vendor/example/include/vendor.h'
        path.parent.mkdir(parents=True)
        path.write_text('/* immutable dependency */\n')
        digest = hashlib.sha256(path.read_bytes()).hexdigest()
        (self.root / 'docs/upstream-files.sha256.md').write_text(
            '# Dependency checksums\n' + digest + '  native/vendor/example/include/vendor.h\n')

    def replace(self, name, before, after):
        path = self.root / name
        text = path.read_text()
        self.assertIn(before, text)
        path.write_text(text.replace(before, after))

    def reject_workflow(self):
        with self.assertRaises((ValueError, AssertionError)):
            workflow_security(self.root)

    def test_current_repository_integrity(self):
        self.assertEqual(check(ROOT)['vendor_files'], 804)

    def test_ordinary_source_and_test_changes_are_not_frozen(self):
        for name in ('native/src/new.cpp', 'ada/src/new.ads', 'rust/src/new.rs',
                     'python/ams_mel/new_module.py', 'python/tests/test_future.py',
                     'python/README.md', 'docs/tests/test_current.py'):
            path = self.root / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text('ordinary new source\n')
        self.assertEqual(check(self.root)['vendor_files'], 1)
        (self.root / 'python/ams_mel/new_module.py').write_text('changed implementation\n')
        self.assertEqual(check(self.root)['vendor_files'], 1)

    def test_vendor_mutation_rejected(self):
        (self.root / 'native/vendor/example/include/vendor.h').write_text('changed\n')
        with self.assertRaisesRegex(ValueError, 'checksum'):
            vendor_integrity(self.root)

    def test_vendor_addition_and_removal_rejected(self):
        extra = self.root / 'native/vendor/extra.h'
        extra.write_text('unlisted dependency\n')
        with self.assertRaisesRegex(ValueError, 'inventory'):
            vendor_integrity(self.root)
        extra.unlink()
        (self.root / 'native/vendor/example/include/vendor.h').unlink()
        with self.assertRaisesRegex(ValueError, 'inventory'):
            vendor_integrity(self.root)

    def test_vendor_symlink_rejected(self):
        (self.root / 'native/vendor/link').symlink_to('/tmp')
        with self.assertRaisesRegex(ValueError, 'symlink'):
            vendor_integrity(self.root)

    def test_duplicate_vendor_checksum_rejected(self):
        path = self.root / 'docs/upstream-files.sha256.md'
        path.write_text(path.read_text() * 2)
        with self.assertRaisesRegex(ValueError, 'duplicate'):
            vendor_integrity(self.root)

    def test_untrusted_pr_token_cannot_write(self):
        self.replace('.github/workflows/ci.yml', 'contents: read', 'contents: write')
        self.reject_workflow()

    def test_job_token_cannot_write(self):
        self.replace('.github/workflows/ci.yml', '  python:\n',
                     '  python:\n    permissions:\n      contents: write\n')
        self.reject_workflow()

    def test_privileged_pr_event_rejected(self):
        self.replace('.github/workflows/ci.yml', '  pull_request:', '  pull_request_target:')
        self.reject_workflow()

    def test_pages_pr_trigger_rejected(self):
        self.replace('.github/workflows/documentation.yml', 'on:\n', 'on:\n  pull_request:\n')
        self.reject_workflow()

    def test_pages_non_main_trigger_rejected(self):
        self.replace('.github/workflows/documentation.yml', 'branches: [main]', 'branches: [feature]')
        self.reject_workflow()

    def test_pages_deploy_guard_cannot_be_bypassed(self):
        self.replace('.github/workflows/documentation.yml',
                     "if: github.ref == 'refs/heads/main' && github.event_name != 'pull_request'",
                     'if: always()')
        self.reject_workflow()

    def test_pages_cannot_deploy_from_bootstrap(self):
        self.replace('.github/workflows/ci.yml', 'jobs:\n',
                     'jobs:\n  bypass:\n    uses: actions/deploy-pages@main\n')
        self.reject_workflow()

    def test_artifacts_must_be_selected_by_same_run_id(self):
        self.replace('.github/workflows/documentation.yml',
                     'artifact-ids: ${{ needs.build.outputs.gate_id }}',
                     'name: arbitrary-untrusted-artifact')
        self.reject_workflow()

    def test_artifact_source_verification_cannot_be_removed(self):
        self.replace('.github/workflows/documentation.yml',
                     '--gate-sha', '--unchecked-gate')
        self.reject_workflow()


if __name__ == '__main__':
    unittest.main()
