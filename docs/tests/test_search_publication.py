"""Independent consumer search contract mutations using current qualification."""

import copy
import json
import os
from pathlib import Path
import sys
import unittest

ROOT = Path(__file__).resolve().parents[2]
sys.path[:0] = [str(ROOT / 'docs/tools'), str(ROOT / 'scripts')]
import build_enhanced_site as p
import search_audit
from local_type_audit import Authority
from check_doc_coverage import measure


class SearchPublicationTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.directory = Path(os.environ.get('MEL_SEARCH_QUALIFICATION',
                                            ROOT / '.doc-tools/search-qualified'))
        cls.site = cls.directory / 'pristine'
        cls.report = measure(cls.site, ROOT)
        cls.authority = Authority(ROOT, cls.site, ledger=cls.report['native'])
        cls.actual = p.files(cls.directory / 'published')

    def qualify(self, actual):
        return search_audit.qualify(ROOT, self.site, self.report, self.authority, actual)

    def test_complete_source_native_identity_authorization(self):
        insertion, report = self.qualify(self.actual)
        self.assertEqual(report['records'], sum(report['counts'].values()))
        self.assertGreater(report['counts']['operation'], 0)
        restored = search_audit.restore(self.actual['api/index.html'].decode(), insertion)
        self.assertNotIn('documentation-search-authority', restored)

    def test_missing_corrupt_forged_and_resealed_index_rejected(self):
        original = self.actual['api/gnatdoc-search-index.json']
        data = json.loads(original)
        for mutation in ('missing', 'corrupt', 'forged', 'private', 'nested', 'omission', 'category', 'duplicate'):
            with self.subTest(mutation=mutation):
                actual = dict(self.actual)
                altered = copy.deepcopy(data)
                if mutation == 'missing':
                    actual.pop('api/gnatdoc-search-index.json')
                elif mutation == 'corrupt':
                    actual['api/gnatdoc-search-index.json'] = original + b' '
                else:
                    record = next(r for r in altered['records'] if r['kind'] == 'operation')
                    if mutation == 'forged':
                        record['href'] = 'https://example.invalid/forged'
                    elif mutation in ('private', 'nested'):
                        record['name'] += '.Private_Forgery' if mutation == 'private' else '.Nested.Forgery'
                    elif mutation == 'omission':
                        altered['records'].remove(record)
                    elif mutation == 'category':
                        record['kind'] = 'symbol'
                    else:
                        altered['records'].append(record)
                    actual['api/gnatdoc-search-index.json'] = json.dumps(altered).encode()
                # Updating the generated output digest cannot authorize it.
                manifest = json.loads(actual['api/presentation.json'])
                manifest['output_sha256'].update(p.hashes(actual))
                actual['api/presentation.json'] = json.dumps(manifest).encode()
                with self.assertRaises((ValueError, KeyError)):
                    self.qualify(actual)

    def test_shell_css_and_router_mutations_rejected(self):
        insertion, _ = self.qualify(self.actual)
        with self.assertRaises(ValueError):
            search_audit.restore(self.actual['api/index.html'].decode().replace(
                'data-sha256="', 'data-sha256="0', 1), insertion)
        for name in ('api/gnatdoc-search.css', 'api/gnatdoc-navigation.js'):
            altered = dict(self.actual)
            altered[name] += b'\n'
            with self.assertRaises(ValueError):
                self.qualify(altered)


if __name__ == '__main__':
    unittest.main()