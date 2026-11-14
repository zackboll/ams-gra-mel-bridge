"""Official wheel identity and fail-closed installed-origin qualification.

Run explicitly with the isolated documentation interpreter, not system Python.
"""

import importlib.metadata
import json
from pathlib import Path
import subprocess
import sys
import unittest
from unittest import mock

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'docs/tools'))
import build_enhanced_site as presentation


class ReleaseTests(unittest.TestCase):
    def test_installed_release_cli_origin_and_complete_contents(self):
        presentation.tool()
        cli = Path(sys.executable).parent / 'gnatdoc-enhancer'
        self.assertEqual(subprocess.check_output([str(cli), '--version'], text=True).strip(),
                         presentation.VERSION)
        distribution = importlib.metadata.distribution('ada-gnatdoc-enhancer')
        origin = json.loads(distribution.read_text('direct_url.json'))
        self.assertEqual(origin['url'], presentation.WHEEL_URL)
        self.assertEqual(origin['archive_info']['hashes']['sha256'], presentation.WHEEL_SHA256)

    def test_wrong_distribution_version_rejected(self):
        with mock.patch.object(presentation.importlib.metadata, 'version', return_value='0.1.0.dev0'):
            with self.assertRaisesRegex(ValueError, 'version mismatch'):
                presentation.tool()

    def test_changed_package_or_assets_rejected(self):
        with mock.patch.object(presentation, 'PACKAGE_SHA256', '0' * 64):
            with self.assertRaisesRegex(ValueError, 'implementation/assets mismatch'):
                presentation.tool()

    def test_missing_local_or_wrong_digest_origin_rejected(self):
        distribution = importlib.metadata.distribution('ada-gnatdoc-enhancer')
        for origin in ({}, {'url': 'file:///tmp/' + presentation.WHEEL_NAME},
                       {'url': presentation.WHEEL_URL,
                        'archive_info': {'hashes': {'sha256': '0' * 64}}}):
            with self.subTest(origin=origin):
                altered = mock.Mock(wraps=distribution)
                altered.version = distribution.version
                altered.read_text.return_value = json.dumps(origin)
                with mock.patch.object(presentation.importlib.metadata, 'distribution', return_value=altered):
                    with self.assertRaisesRegex(ValueError, 'wheel origin/digest mismatch'):
                        presentation.tool()

    def test_shadowed_import_origin_rejected(self):
        distribution = importlib.metadata.distribution('ada-gnatdoc-enhancer')
        altered = mock.Mock(wraps=distribution)
        altered.version = distribution.version
        altered.locate_file.return_value = ROOT / 'not-the-installed-package'
        with mock.patch.object(presentation.importlib.metadata, 'distribution', return_value=altered):
            with self.assertRaisesRegex(ValueError, 'import origin mismatch'):
                presentation.tool()


if __name__ == '__main__':
    unittest.main()