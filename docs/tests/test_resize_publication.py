"""Independent resize mutations, including forged/resealed output manifests."""

import json
import os
from pathlib import Path
import sys
import unittest

ROOT = Path(__file__).resolve().parents[2]
sys.path[:0] = [str(ROOT / 'docs/tools'), str(ROOT / 'scripts')]
import build_enhanced_site as p
import resize_audit as resize
import search_audit
import source_epoch as epoch
from check_doc_coverage import measure
from example_sources import References
from local_type_audit import Authority
from check_live_docs_site import graph


class ResizePublicationTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.directory = Path(os.environ.get('MEL_RESIZE_QUALIFICATION',
                                            ROOT / '.doc-tools/resize-qualified'))
        cls.site = cls.directory / 'pristine'
        cls.actual = p.files(cls.directory / 'published')
        cls.report = measure(cls.site, ROOT)
        cls.authority = Authority(ROOT, cls.site, ledger=cls.report['native'])

    def test_reverse_feature_order_recovers_entire_native_index(self):
        insertion, _ = search_audit.qualify(ROOT, self.site, self.report, self.authority, self.actual)
        text = search_audit.restore(resize.qualify(self.actual), insertion)
        import symbol_audit
        text = p.restore_navigation(text, symbol_audit.inventory(self.authority, self.site))
        original = (self.site / 'api/index.html').read_bytes()
        self.assertEqual(p.restore_document(text, original.decode(), 'api/index.html',
                                            self.authority).encode(), original)
        self.assertTrue(graph(self.actual))

    def test_resealed_shell_mutations_rejected(self):
        original = self.actual['api/index.html'].decode()
        additions = (resize.HEAD, resize.SIDEBAR, resize.DIVIDER)
        mutations = {}
        for index, value in enumerate(additions):
            mutations[f'missing-{index}'] = original.replace(value, '')
            mutations[f'duplicate-{index}'] = original.replace(value, value + value)
            mutations[f'misplaced-{index}'] = original.replace(value, '') + value
        for token in ('role="separator"', 'aria-orientation="vertical"',
                      'aria-label="Navigation width"', 'aria-controls="resizable-navigation"',
                      'tabindex="0"', 'hidden></div>', 'id="resizable-navigation"',
                      'src="gnatdoc-resize.js"', 'href="gnatdoc-resize.css"'):
            mutations[token] = original.replace(token, token + ' data-forged="true"', 1)
        style, script = resize.HEAD.split('<script', 1)
        mutations['reordered-head'] = original.replace(resize.HEAD, '<script' + script + style)
        mutations['extra-separator'] = original + '<div id="navigation-resize-forged"></div>'
        mutations['forged-resource'] = original.replace('src="gnatdoc-resize.js"',
                                                       'src="https://evil.invalid/resize.js"')
        for label, text in mutations.items():
            with self.subTest(mutation=label):
                actual = dict(self.actual, **{'api/index.html': text.encode()})
                self.reseal(actual)
                with self.assertRaises(ValueError):
                    resize.qualify(actual)
                with self.assertRaises(ValueError):
                    graph(actual)

    @staticmethod
    def reseal(actual):
        manifest = json.loads(actual['api/presentation.json'])
        manifest['output_sha256'] = p.hashes({n: d for n, d in actual.items()
            if n not in {'api/presentation.json', 'api/gnatdoc.css'}})
        actual['api/presentation.json'] = json.dumps(manifest).encode()

    def test_missing_changed_resealed_assets_and_manifest_identities_rejected(self):
        for name in resize.ASSETS:
            for mutation in ('missing', 'changed', 'forged-manifest', 'missing-manifest'):
                with self.subTest(asset=name, mutation=mutation):
                    actual = dict(self.actual)
                    if mutation == 'missing':
                        del actual[name]
                        self.reseal(actual)
                    elif mutation == 'changed':
                        actual[name] += b'\n'
                        self.reseal(actual)
                    else:
                        manifest = json.loads(actual['api/presentation.json'])
                        if mutation == 'forged-manifest':
                            manifest['output_sha256'][name] = '0' * 64
                        else:
                            del manifest['output_sha256'][name]
                        actual['api/presentation.json'] = json.dumps(manifest).encode()
                    with self.assertRaises(ValueError):
                        resize.qualify(actual)

    def test_complete_independent_reconstruction(self):
        references = References(ROOT, self.site, ledger=self.report['native'])
        result = epoch.verify_presentation(self.site, ROOT, self.report, references,
                                           self.directory / 'published', deep=True)
        self.assertEqual(result['files'], p.hashes(self.actual))


if __name__ == '__main__':
    unittest.main()