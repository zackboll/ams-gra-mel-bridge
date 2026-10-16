"""Committed-fixture qualification of DOC-001 source-integrity boundaries."""

import subprocess
import tempfile
import unittest
from pathlib import Path

from check_doc_source_integrity import check


class SourceIntegrityTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.git('init', '-q')
        self.git('config', 'user.name', 'Documentation Fixture')
        self.git('config', 'user.email', 'fixture@example.invalid')
        self.write('native/src/existing.cpp', 'int value = 1;\n')
        self.write('ada/src/demo.ads', 'package Demo is\nend Demo;\n')
        self.commit()
        self.base = self.git('rev-parse', 'HEAD').decode().strip()

    def git(self, *args):
        return subprocess.check_output(['git', '-C', str(self.root), *args],
                                       stderr=subprocess.STDOUT)

    def write(self, name, content):
        path = self.root / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(content)
        return path

    def commit(self):
        self.git('add', '.')
        self.git('commit', '-qm', 'Committed fixture')
        self.git('diff', '--exit-code')
        self.git('diff', '--cached', '--exit-code')
        self.assertEqual(self.git('status', '--porcelain'), b'')

    def test_reject_committed_unauthorized_additions(self):
        additions = {
            'native/src/new.cpp': 'int added = 1;\n',
            'rust/src/new.rs': 'pub fn added() {}\n',
            'python/ams_mel/new.py': 'added = True\n',
            'ada/src/new.ads': 'package New_Unit is end New_Unit;\n',
            'ada/src/new.adb': 'package body New_Unit is end New_Unit;\n',
            'ada/tests/src/new.ads': 'package New_Test is end New_Test;\n',
            'ada/tests/src/new.adb': 'package body New_Test is end New_Test;\n',
            'integration/new.sh': '#!/bin/sh\nexit 0\n',
            '.github/workflows/unexpected.yml': 'name: Unexpected\n',
            'scripts/unrelated.sh': '#!/bin/sh\nexit 0\n',
            'scripts/unrelated.py': 'added = True\n',
            'unrelated.txt': 'Unrelated top-level file.\n',
            'unrelated.gpr': 'project Unrelated is end Unrelated;\n',
            'native/src/line\nbreak.cpp': 'int added = 1;\n',
        }
        for name, content in additions.items():
            with self.subTest(path=name):
                path = self.write(name, content)
                if name.endswith('.sh'):
                    path.chmod(0o755)
                self.commit()
                try:
                    with self.assertRaisesRegex(ValueError, 'unauthorized tracked files added'):
                        check(self.root, self.base)
                finally:
                    path.unlink()
                    self.commit()

    def test_accept_committed_documentation_additions(self):
        for name in ('docs/guide.md', 'docs/toolchain/alire.toml',
                     'docs_api.gpr', 'docs_internal.gpr',
                     'scripts/check_doc_source_integrity.py',
                     'scripts/test_doc_source_integrity.py',
                     'scripts/build_docs_portal.py', 'scripts/test_docs_portal.py',
                     'scripts/generate_docs_baseline.sh',
                     '.github/workflows/documentation.yml'):
            with self.subTest(path=name):
                self.write(name, 'Documentation fixture.\n')
                self.commit()
                self.assertEqual(check(self.root, self.base)['protected_files'], 2)

    def test_existing_production_bytes_remain_protected(self):
        self.write('native/src/existing.cpp', 'int value = 2;\n')
        self.commit()
        with self.assertRaisesRegex(ValueError, 'production bytes changed'):
            check(self.root, self.base)

    def test_ada_comments_allowed_but_tokens_protected(self):
        self.write('ada/src/demo.ads', '-- Documentation.\npackage Demo is\nend Demo;\n')
        self.commit()
        check(self.root, self.base)
        self.write('ada/src/demo.ads', 'package Demo is\n   X : Integer;\nend Demo;\n')
        self.commit()
        with self.assertRaisesRegex(ValueError, 'executable Ada tokens changed'):
            check(self.root, self.base)

    def test_existing_production_removal_rejected(self):
        (self.root / 'native/src/existing.cpp').unlink()
        self.commit()
        with self.assertRaisesRegex(ValueError, 'production file removed'):
            check(self.root, self.base)

    def test_existing_ada_removal_rejected(self):
        (self.root / 'ada/src/demo.ads').unlink()
        self.commit()
        with self.assertRaisesRegex(ValueError, 'Ada source inventory changed'):
            check(self.root, self.base)


if __name__ == '__main__':
    unittest.main()
