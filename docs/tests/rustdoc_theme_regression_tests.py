"""Explicit real-toolchain reproducibility and regenerated-forgery qualification."""

import copy
from pathlib import Path
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
sys.path[:0] = [str(ROOT / 'docs/tools'), str(ROOT / 'scripts')]
import docs_ci as ci
import rustdoc_publication as rust


class GeneratedThemeTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        temp = tempfile.TemporaryDirectory(prefix='rustdoc-theme-regression-')
        cls.addClassCleanup(temp.cleanup)
        cls.directory = Path(temp.name)
        cls.context = ci.identity(ROOT)
        cls.exported = cls.directory / 'source'
        ci.epoch.export(ROOT, cls.context['commit'], cls.exported)
        cls.files, cls.record = rust.generate(cls.exported, cls.directory / 'first', cls.context,
                                             cls.directory / 'first.log')

    def test_two_independent_exports_are_byte_identical(self):
        exported = self.directory / 'second-source'
        ci.epoch.export(ROOT, self.context['commit'], exported)
        rust.verify_regenerated(self.files, self.record, exported, self.directory / 'second',
                                self.context, self.directory / 'second.log')

    def test_real_missing_and_changed_source_fail(self):
        theme = self.exported / rust.THEME_SOURCE
        data = theme.read_bytes()
        try:
            theme.unlink()
            with self.assertRaisesRegex(ValueError, 'missing Rustdoc theme'):
                rust.generate(self.exported, self.directory / 'missing', self.context, self.directory / 'log')
            theme.write_bytes(data + b'\n/* changed */\n')
            with self.assertRaisesRegex(ValueError, 'source identity'):
                rust.generate(self.exported, self.directory / 'changed', self.context, self.directory / 'log')
        finally:
            theme.write_bytes(data)

    def test_output_forgery_with_self_consistent_record_is_rejected_by_rebuild(self):
        files = dict(self.files)
        name = next(n for n in files if n.startswith('static.files/main-') and n.endswith('.js'))
        files[name] += b'\n/* output-side forgery */\n'
        record = copy.deepcopy(self.record)
        record['files'] = rust.publication.hashes(files)
        rust.verify_record(files, record, self.context)  # Demonstrate why seals alone are not reconstruction.
        with self.assertRaisesRegex(ValueError, 'independently regenerated'):
            rust.verify_regenerated(files, record, self.exported, self.directory / 'forgery',
                                    self.context, self.directory / 'forgery.log')

    def test_actual_extra_css_and_modified_theme_rejected(self):
        files = dict(self.files, **{'unauthorized.css': b'body {color:red}'})
        record = dict(self.record, files=rust.publication.hashes(files), counts=rust.validate(files))
        with self.assertRaisesRegex(ValueError, 'independently regenerated'):
            rust.verify_regenerated(files, record, self.exported, self.directory / 'extra-css',
                                    self.context, self.directory / 'extra-css.log')
        files = dict(self.files, **{'theme.css': b'body {color:red}'})
        record = dict(self.record, files=rust.publication.hashes(files))
        with self.assertRaisesRegex(ValueError, 'generated theme'):
            rust.verify_record(files, record, self.context)

    def test_actual_missing_output_resource_fails_even_with_rehashed_record(self):
        files = dict(self.files)
        name = next(n for n in files if n.startswith('static.files/rustdoc-') and n.endswith('.css'))
        del files[name]
        record = dict(self.record, files=rust.publication.hashes(files))
        with self.assertRaisesRegex(ValueError, 'missing Rustdoc destination'):
            rust.verify_record(files, record, self.context)


if __name__ == '__main__':
    unittest.main()
