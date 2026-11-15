"""Current-source schema-3 qualification evidence, not historical count gates."""

import json
import os
from pathlib import Path
import sys
import unittest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'docs/tools'))
import authored_html
import build_enhanced_site as p
import feature_source


class AuthoredHTMLTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.directory = Path(os.environ['MEL_AUTHORED_QUALIFICATION'])
        cls.evidence = json.loads((cls.directory / 'evidence.json').read_bytes())

    def test_exact_current_source_and_official_wheel(self):
        self.assertEqual(self.evidence['commit'], feature_source.revision(ROOT, 'HEAD'))
        self.assertEqual(self.evidence['wheel_sha256'], p.WHEEL_SHA256)
        self.assertEqual(self.evidence['package_sha256'], p.PACKAGE_SHA256)

    def test_prior_eighteen_and_schema_three_negatives(self):
        negatives = self.evidence['negative_cases']
        self.assertGreaterEqual(len(negatives), 32)
        self.assertTrue(all(r['rejected'] for r in negatives.values()))
        self.assertTrue({'resealed-substituted-source', 'resealed-wrong-main-owner',
                         'unbound-public-source', 'wrong-guide-ownership', 'token-in-comment',
                         'token-in-attribute', 'token-in-code', 'tampered-complete-listing'} <= set(negatives))

    def test_no_partial_overlay_or_extra_presentation_features(self):
        portal = authored_html.manifest(ROOT)
        expected = {portal['home'], portal['stylesheet'], *(g['path'] for g in portal['guides'])}
        self.assertTrue(expected <= set(self.evidence['inventory']))
        self.assertFalse(any('resize' in n or 'search.js' in n for n in self.evidence['inventory']))

    def test_authored_manifest_binds_all_public_sources(self):
        from gnatdoc_inventory import specifications
        data = json.loads((self.directory / 'authored-manifest.json').read_bytes())
        self.assertEqual(data['schema_version'], 3)
        self.assertEqual(data['source_blocks']['profile'], 'executable-ada-catalog-v1')
        for path in specifications(ROOT)[0].values():
            name = path.relative_to(ROOT).as_posix()
            self.assertEqual(data['inputs'][name], p.sha(path.read_bytes()))

    def test_required_nine_asset_parity_when_baseline_supplied(self):
        parity = self.evidence['portal_parity']
        if os.environ.get('MEL_REQUIRE_BASELINE_PARITY') == '1':
            self.assertEqual(len(parity), 9)
        self.assertTrue(all(r['identical'] for r in parity.values()))


if __name__ == '__main__':
    unittest.main()
