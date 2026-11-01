"""Actual current-source GNATdoc regeneration and artifact mutation controls.

Explicit qualification needs the pinned documentation environment; it is not a
provider-free Bootstrap dependency and never substitutes today's tests into an
immutable historical checkout.
"""

import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest
from unittest import mock

ROOT = Path(__file__).resolve().parents[2]
sys.path[:0] = [str(ROOT / 'docs/tools'), str(ROOT / 'scripts')]
import build_enhanced_site as presentation
import feature_source as source
import source_epoch as epoch
import docs_ci


class GeneratedSourceTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp = tempfile.TemporaryDirectory(prefix='current-source-doc-regression-')
        cls.addClassCleanup(cls.temp.cleanup)
        cls.directory = Path(cls.temp.name)
        cls.repo = cls.directory / 'source'
        subprocess.run(['git', 'clone', '-q', '--shared', '--no-checkout', str(ROOT), str(cls.repo)], check=True)
        cls.git('checkout', '-q', '--detach', source.revision(ROOT, 'HEAD'))
        cls.git('config', 'user.name', 'Documentation Regression Fixture')
        cls.git('config', 'user.email', 'fixture@example.invalid')
        cls.target = cls.git('rev-parse', 'HEAD')
        path = cls.repo / 'ada/src/ams-mel-rf-product_rx.ads'
        cls.original = path.read_text()
        addition = ('\n   subtype Qualification_Observation_Count is Natural;\n'
                    '   --  Count observed values without retaining a native owner.\n')
        if cls.original.count('\nprivate\n') != 1:
            raise ValueError('test requires one private section in Product_Rx specification')
        path.write_text(cls.original.replace('\nprivate\n', addition + '\nprivate\n'))
        cls.git('add', '-A')
        cls.git('commit', '-qm', 'ordinary documented Ada addition')
        cls.candidate = cls.git('rev-parse', 'HEAD')
        cls.prefix = epoch.toolchain(os.environ['GNATDOC_PREFIX'])
        cls.output = cls.directory / 'epoch'
        cls.value = epoch.qualify(cls.repo, cls.target, cls.candidate, cls.output, cls.prefix)

    @classmethod
    def git(cls, *args):
        return subprocess.check_output(['git', '-C', str(cls.repo), *args],
                                       stderr=subprocess.STDOUT, text=True).strip()

    def copy_epoch(self):
        temp = tempfile.TemporaryDirectory(dir=self.directory)
        self.addCleanup(temp.cleanup)
        destination = Path(temp.name) / 'epoch'
        shutil.copytree(self.output, destination)
        return destination

    def test_documented_addition_uses_actual_source_anchor_and_catalog(self):
        self.assertEqual(self.value['status'], 'mechanically-qualified')
        self.assertEqual(self.value['candidate_commit'], self.candidate)
        added = next(item for item in self.value['publication']['symbols']
                     if item['canonical'].endswith('.Qualification_Observation_Count'))
        text = (self.output / 'published/guides/api-map.html').read_text()
        self.assertIn(added['page'] + '#' + added['anchor'], text)
        self.assertIn('Qualification_Observation_Count',
                      (self.output / 'pristine/api' / added['page']).read_text())

    def test_wrong_source_and_manifest_identity_rejected(self):
        with self.assertRaises(ValueError):
            epoch.verify(self.repo, self.output, self.target, self.target)
        altered = self.copy_epoch()
        value = json.loads((altered / 'epoch.json').read_text())
        value['candidate_tree'] = '0' * 40
        (altered / 'epoch.json').write_text(json.dumps(value))
        with self.assertRaisesRegex(ValueError, 'source/manifest'):
            epoch.verify(self.repo, altered, self.target, self.candidate)

    def test_changed_publication_and_pristine_bytes_rejected(self):
        for name in ('published/api/index.html', 'pristine/api/index.html'):
            with self.subTest(path=name):
                altered = self.copy_epoch()
                path = altered / name
                path.write_bytes(path.read_bytes() + b'<!-- unqualified content -->')
                with self.assertRaisesRegex(ValueError, 'artifact mismatch'):
                    epoch.verify(self.repo, altered, self.target, self.candidate)

    def test_forged_consistent_artifact_hashes_fail_independent_regeneration(self):
        altered = self.copy_epoch()
        path = altered / 'published/api/index.html'
        path.write_bytes(path.read_bytes() + b'<!-- forged content -->')
        value = json.loads((altered / 'epoch.json').read_text())
        value['publication']['files'] = presentation.hashes(presentation.files(altered / 'published'))
        (altered / 'epoch.json').write_text(json.dumps(value))
        # Self-consistency alone is explicitly insufficient.
        epoch.verify(self.repo, altered, self.target, self.candidate)
        with self.assertRaisesRegex(ValueError, 'independently regenerated'):
            epoch.verify_regenerated(self.repo, altered, self.target, self.candidate, self.prefix)

    def test_normal_verifier_rejects_corruption_even_with_recomputed_payload_hashes(self):
        from check_doc_coverage import measure
        with tempfile.TemporaryDirectory(dir=self.directory) as temp:
            directory = Path(temp) / 'normal'
            directory.mkdir()
            for name in ('pristine', 'published'):
                shutil.copytree(self.output / name, directory / name)
            report = measure(directory / 'pristine', self.repo)
            docs_ci.write(directory / 'coverage.json', report)
            docs_ci.write(directory / 'native-anchors.json', report['native'])
            with mock.patch.dict(os.environ, {'GITHUB_ACTIONS': 'false'}):
                manifest = {'schema': 1, 'identity': docs_ci.identity(self.repo),
                            'payload': presentation.hashes(presentation.files(directory))}
                docs_ci.write(directory / 'manifest.json', manifest)
                digest = epoch.sha((directory / 'manifest.json').read_bytes())
                docs_ci.verify(self.repo, directory, digest)
                for name, replacement in (
                    ('published/api/index.html', lambda data: data + b'<!-- forged -->'),
                    ('published/guides/getting-started.html', lambda data: data + b'<a href="missing.html">broken</a>'),
                    ('published/api/gnatdoc-navigation.js', lambda data: data + b'// modified'),
                    ('pristine/api/index.html', lambda data: data.replace(b'gnatdoc.css', b'missing.css')),
                ):
                    with self.subTest(path=name):
                        path = directory / name
                        original = path.read_bytes()
                        path.write_bytes(replacement(original))
                        content = presentation.files(directory)
                        del content['manifest.json']
                        manifest['payload'] = presentation.hashes(content)
                        docs_ci.write(directory / 'manifest.json', manifest)
                        digest = epoch.sha((directory / 'manifest.json').read_bytes())
                        with self.assertRaises(ValueError):
                            docs_ci.verify(self.repo, directory, digest)
                        path.write_bytes(original)

    def test_undocumented_addition_is_reported_without_blocking_generation(self):
        temp = tempfile.TemporaryDirectory(dir=self.directory)
        self.addCleanup(temp.cleanup)
        directory = Path(temp.name)
        repo = directory / 'source'
        subprocess.run(['git', 'clone', '-q', '--shared', str(self.repo), str(repo)], check=True)
        path = repo / 'ada/src/ams-mel-rf-product_rx.ads'
        path.write_text(self.original.replace('\nprivate\n',
                        '\n   subtype Undocumented_Count is Natural;\n'
                        '   function Undocumented_Value (Input : Natural) return Natural;\n\nprivate\n'))
        subprocess.run(['git', '-C', str(repo), 'add', '-A'], check=True)
        subprocess.run(['git', '-C', str(repo), '-c', 'user.name=Documentation Test',
                        '-c', 'user.email=fixture@example.invalid', 'commit', '-qm', 'undocumented addition'], check=True)
        site, report, references = epoch.generate(
            repo, 'HEAD', directory / 'export', self.prefix, directory / 'generation.log')
        self.assertTrue(site.is_dir())
        gaps = [e for e in report['elements'] if not e['documented']]
        self.assertTrue(any('Undocumented_Count' in e['identity'] for e in gaps))
        self.assertIn('Undocumented_Count', (directory / 'export/obj/docs/gnatdoc.log').read_text())


if __name__ == '__main__':
    unittest.main()
