"""Authentic current-source pilot regression contract; no historical count gates.

Run with MEL_AUTHORED_PILOT_BASELINE pointing to a fresh docs_ci qualify result.
"""

import os
import json
from pathlib import Path
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'docs/tools'))
import authored_html_pilot as pilot


@unittest.skipUnless(os.environ.get('MEL_AUTHORED_PILOT_BASELINE'),
                     'explicit fresh source-epoch baseline required')
class AuthoredHTMLPilotTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.workspace = tempfile.TemporaryDirectory(prefix='mel-authored-tests-')
        cls.addClassCleanup(cls.workspace.cleanup)
        cls.output = Path(cls.workspace.name) / 'probe'
        cls.evidence = pilot.probe(
            ROOT, os.environ.get('MEL_AUTHORED_PILOT_COMMIT', 'HEAD'),
            Path(os.environ['MEL_AUTHORED_PILOT_BASELINE']), cls.output)

    def test_all_seven_authentic_guides_are_classified(self):
        expected = pilot.manifest(ROOT)
        rows = self.evidence['guides']
        self.assertEqual(set(rows), {expected['home'], expected['stylesheet'],
                                     *(g['path'] for g in expected['guides'])})
        blocked = {n for n, r in rows.items() if r['status'] != 'directly-supported'}
        self.assertEqual(blocked, {'guides/examples.html', 'guides/api-map.html'})
        self.assertTrue(all(r['identical'] for n, r in rows.items() if n not in blocked))

    def test_no_fake_complete_publication_or_production_switch(self):
        self.assertFalse(self.evidence['full_pipeline']['completed'])
        self.assertFalse(self.evidence['migration_ready'])
        self.assertEqual(self.evidence['decision'], 'small-shared-enhancer-extension-required')
        config = (ROOT / 'docs/gnatdoc-enhance.toml').read_text()
        self.assertIn('[guides]\nmode = "none"', config)
        for name in ('examples', 'api-map'):
            self.assertFalse((self.output / 'candidate-accepted-only/guides' / (name + '.html')).exists())

    def test_exact_markup_gaps_remain_visible(self):
        self.assertEqual(set(self.evidence['markup']),
                         {'details', 'summary', 'example-source', 'catalog', 'source-end'})
        self.assertTrue(all(not r['supported'] for r in self.evidence['markup'].values()))

    def test_adversarial_cases_fail_closed(self):
        required = {
            'manipulated-contract-digest', 'duplicate-guide-identity',
            'resealed-group-destination', 'resealed-native-digest',
            'missing-guide-source', 'symlink-guide-source', 'resealed-substituted-source',
            'resealed-altered-anchor-destination', 'unsafe-script', 'unsafe-event',
            'unsafe-uri', 'arbitrary-expansion', 'forged-catalog-entry',
            'incorrect-rendered-example-source', 'substituted-executable-source',
            'unexpected-example-file', 'unexpected-native-file', 'unexpected-publication-file'}
        self.assertEqual(set(self.evidence['negative_cases']), required)
        self.assertTrue(all(r['rejected'] for r in self.evidence['negative_cases'].values()))

    def test_inventory_is_current_source_not_historical_counts(self):
        self.assertEqual(set(self.evidence['public_packages']), set(pilot.specifications(ROOT)[0]))
        epoch = json.loads((Path(os.environ['MEL_AUTHORED_PILOT_BASELINE'])
                            / 'epoch.json').read_bytes())
        self.assertEqual(self.evidence['native'], epoch['native'])

    def test_api_only_control_remains_identical_without_claiming_full_candidate(self):
        expected = {n: digest for n, digest in self.evidence['production_sha256'].items()
                    if n.startswith('api/')}
        self.assertEqual(self.evidence['api_control_sha256'], expected)


if __name__ == '__main__':
    unittest.main()
