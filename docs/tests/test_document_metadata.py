"""Consumer-owned serialized metadata guards on authentic MEL publication."""

import sys
import json
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'docs/tools'))
import build_enhanced_site as p
from ada_gnatdoc_enhancer.config import load
from ada_gnatdoc_enhancer.pipeline import expected_output


class MetadataTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.authority = p.Authority(ROOT, ROOT / '_site')
        _, cls.output, _ = expected_output(load(ROOT, 'docs/gnatdoc-enhance.toml'), ROOT / '_site')
        cls.name = 'api/' + next(n for n, pkg in cls.authority.page_packages.items()
                                if pkg == 'AMS.MEL.IR.Image')
        cls.title = 'AMS.MEL.IR.Image — AMS MEL Bridge'
        cls.text = cls.output[cls.name].decode()
        cls.native = (ROOT / '_site' / cls.name).read_text()

    def reject(self, text):
        # Directly reach the intended serialized metadata guard: no hash,
        # fingerprint, manifest or enhancer verifier can explain rejection.
        with self.assertRaisesRegex(p.PresentationError, 'document (metadata|head)'):
            p.restore_document(text, self.native, self.name, self.authority)

    def test_all_thirty_source_owned_titles_and_exact_restoration(self):
        for name, data in self.output.items():
            if not name.endswith('.html'):
                continue
            text = data.decode()
            if name == 'api/index.html':
                text = p.restore_navigation(text, p.symbol_audit.inventory(self.authority, ROOT / '_site'))
                title = 'API Reference — AMS MEL Bridge'
            else:
                title = self.authority.page_packages[Path(name).name] + ' — AMS MEL Bridge'
            self.assertIn('<title>' + title + '</title>', text)
            native = (ROOT / '_site' / name).read_text()
            restored = p.restore_document(text, native, name, self.authority)
            restored, _ = p.restore_page(Path(name).name, restored, self.authority)
            self.assertEqual(restored.encode(), (ROOT / '_site' / name).read_bytes())

    def test_filename_does_not_grant_package_identity(self):
        import copy
        authority = copy.deepcopy(self.authority)
        del authority.page_packages[Path(self.name).name]
        with self.assertRaisesRegex(p.PresentationError, 'source package identity'):
            p.restore_document(self.text, self.native, self.name, authority)

    def test_correct_index_script_is_inside_head(self):
        text = self.output['api/index.html'].decode()
        self.assertEqual(text.count(p.NAVIGATION_SCRIPT + '</head><body class=main>'), 1)
        symbols = p.symbol_audit.inventory(self.authority, ROOT / '_site')
        p.restore_document(p.restore_navigation(text, symbols), (ROOT / '_site/api/index.html').read_text(),
                           'api/index.html', self.authority)

    def test_forged_self_consistent_manifest_cannot_authorize_wrong_title(self):
        with tempfile.TemporaryDirectory() as tmp:
            site = Path(tmp) / 'site'
            p.build(ROOT / '_site', site)
            path = site / self.name
            path.write_text(path.read_text().replace(self.title, 'Incorrect — AMS MEL Bridge', 1))
            manifest = site / 'api/presentation.json'
            data = json.loads(manifest.read_text())
            data['output_sha256'][self.name] = p.sha(path.read_bytes())
            manifest.write_text(json.dumps(data))
            with self.assertRaisesRegex(p.PresentationError, 'document metadata'):
                p.validate(site, ROOT / '_site')


MUTATIONS = {
    'missing_head': ('<head>', ''),
    'duplicate_head': ('<head>', '<head><head>'),
    'misplaced_head': ('<head>', '<body><head>'),
    'missing_head_end': ('</head>', ''),
    'duplicate_head_end': ('</head>', '</head></head>'),
    'missing_title': ('<title>AMS.MEL.IR.Image — AMS MEL Bridge</title>', ''),
    'wrong_title': ('<title>', '<title>Incorrect '),
    'duplicate_title': ('</title>', '</title><title>AMS.MEL.IR.Image — AMS MEL Bridge</title>'),
    'title_attributes': ('<title>', '<title id="title">'),
    'title_after_head': ('</head>', '</head><title>Extra</title>'),
    'missing_language': (' lang="en"', ''),
    'wrong_language': ('lang="en"', 'lang="fr"'),
    'duplicate_language': ('lang="en"', 'lang="en" lang="en"'),
    'root_attributes': ('lang="en"', 'lang="en" onclick="run()"'),
    'duplicate_root': ('<head>', '<html lang="en"><head>'),
    'missing_charset': ('<meta charset="utf-8">', ''),
    'wrong_charset': ('charset="utf-8"', 'charset="iso-8859-1"'),
    'duplicate_charset': ('<meta charset="utf-8">', '<meta charset="utf-8"><meta charset="utf-8">'),
    'misplaced_charset': ('</head>', '</head><meta charset="utf-8">'),
    'extra_meta': ('</title>', '</title><meta name="author" content="x">'),
    'redirect_meta': ('</title>', '</title><meta http-equiv="refresh" content="0;url=https://example.org">'),
    'viewport_changed': ('initial-scale=1', 'initial-scale=2'),
    'native_stylesheet': ('href=gnatdoc.css', 'href=other.css'),
    'remote_stylesheet': ('href=gnatdoc.css', 'href=https://example.org/gnatdoc.css'),
    'theme_stylesheet': ('href="gnatdoc-theme.css"', 'href="other.css"'),
    'package_script': ('</head>', p.NAVIGATION_SCRIPT + '</head>'),
    'late_package_script': ('<body class=content>', '<body class=content>' + p.NAVIGATION_SCRIPT),
    'inline_title_attribute': ('<title>', '<title onload="run()">'),
    'nested_looking_markup': ('AMS.MEL.IR.Image —', 'AMS.MEL.IR.Image<b> —'),
    'nested_looking_comment': ('AMS.MEL.IR.Image —', 'AMS.MEL.IR.Image<!--hidden--> —'),
    'rcdata_closure': ('AMS.MEL.IR.Image —', 'AMS.MEL.IR.Image</title><script>run()</script><title> —'),
    'literal_bidi': ('AMS.MEL.IR.Image —', 'AMS.MEL.IR.Image\u202e —'),
    'numeric_bidi': ('AMS.MEL.IR.Image —', 'AMS.MEL.IR.Image&#x202e; —'),
    'numeric_zero_width': ('AMS.MEL.IR.Image —', 'AMS.MEL.IR.Image&#8203; —'),
    'numeric_control': ('AMS.MEL.IR.Image —', 'AMS.MEL.IR.Image&#0; —'),
    'numeric_title_character': ('AMS.MEL.IR.Image —', '&#65;MS.MEL.IR.Image —'),
    'extra_head_in_body': ('<body class=content>', '<body class=content><head></head>'),
    'extra_title_in_body': ('<body class=content>', '<body class=content><title>Extra</title>'),
}


def mutation_test(old, new):
    def test(self):
        self.assertIn(old, self.text)
        self.reject(self.text.replace(old, new, 1))
    return test


for name, pair in MUTATIONS.items():
    setattr(MetadataTests, 'test_' + name, mutation_test(*pair))
