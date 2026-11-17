"""Real complete publications: same-source toggles, feature deltas and tampering.

Explicit pinned-toolchain suite, not a provider-free Bootstrap dependency.
Fixtures commit genuine source changes in isolated clones; no output record is
invented and no production binding source in the caller checkout is modified.
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
import build_enhanced_site as p
import docs_ci
import resize_parity as parity
import source_epoch as epoch


class SameSourceParityTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp = tempfile.TemporaryDirectory(prefix='resize-parity-regression-')
        cls.addClassCleanup(cls.temp.cleanup)
        cls.workspace = Path(cls.temp.name)
        cls.candidate = Path(os.environ['MEL_RESIZE_CANDIDATE'])
        cls.digest = epoch.sha((cls.candidate / 'manifest.json').read_bytes())
        cls.actual = p.files(cls.candidate / 'published')
        # Retain actual renderer output for direct exact-set adversarial checks.
        with mock.patch.object(parity, 'compare', wraps=parity.compare) as comparison:
            cls.result = parity.qualify(ROOT, cls.candidate, cls.workspace / 'parity.json', cls.digest)
        cls.disabled = comparison.call_args.args[0]
        cls.prefix = epoch.toolchain(os.environ['GNATDOC_PREFIX'])

    def test_identical_source_complete_publication(self):
        result = self.result
        self.assertTrue(result['passed'])
        self.assertEqual(set(result['delta']), parity.DELTA)
        self.assertEqual(result['disabled_complete_inventory'], p.hashes(self.disabled))
        self.assertEqual(result['candidate'], p.hashes(self.actual))
        for name in self.disabled.keys() - parity.DELTA:
            self.assertEqual(self.disabled[name], self.actual[name], name)
        self.assertEqual(result['identity'], docs_ci.identity(ROOT))
        self.assertEqual(result['disabled_control']['mode'], 'modified-export-not-deployable')
        for name in ('native-anchors.json', 'rustdoc.json', 'python-reference.json'):
            self.assertEqual(result['records'][name], p.sha((self.candidate / name).read_bytes()))

    def test_exact_delta_rejects_extra_missing_or_subset_changes(self):
        for name in ('api/unauthorized.html', 'python/unauthorized.html', 'rust/unauthorized.html'):
            with self.subTest(extra=name), self.assertRaisesRegex(ValueError, 'exact four-file'):
                parity.compare(self.disabled, dict(self.actual, **{name: b'unauthorized'}))
        for name in parity.DELTA:
            with self.subTest(missing=name):
                actual = dict(self.actual)
                del actual[name]
                with self.assertRaises(ValueError):
                    parity.compare(self.disabled, actual)
            with self.subTest(subset=name):
                actual = dict(self.actual)
                if name in self.disabled:
                    actual[name] = self.disabled[name]
                else:
                    del actual[name]
                with self.assertRaisesRegex(ValueError, 'exact four-file'):
                    parity.compare(self.disabled, actual)
        for name in ('python/index.html', 'rust/ams_mel/index.html', 'api/gnatdoc-search-index.json'):
            with self.subTest(unauthorized=name):
                actual = dict(self.actual)
                actual[name] += b'<!-- unexpected toggle effect -->'
                with self.assertRaisesRegex(ValueError, 'exact four-file'):
                    parity.compare(self.disabled, actual)

    def test_resealed_complete_publication_mutations_rejected(self):
        directory = self.workspace / 'mutated'
        shutil.copytree(self.candidate, directory)
        try:
            original = p.files(directory)
            html = next(n for n in self.actual if n.startswith('api/') and
                        n.endswith('.html') and n != 'api/index.html')
            mutations = [(f'published/{html}', lambda d: d + b'<!-- forged -->'),
                         ('published/api/gnatdoc-search-index.json', lambda d: d + b' '),
                         ('published/api/gnatdoc-resize.js', lambda d: d + b'\n'),
                         ('published/api/gnatdoc-resize.css', lambda d: d + b'\n'),
                         ('published/api/presentation.json', lambda d: epoch.canonical({**json.loads(d), 'forged': True})),
                         ('manifest.json', lambda d: epoch.canonical({**json.loads(d), 'identity': {
                             **json.loads(d)['identity'], 'tree': '0' * 40}})),
                         ('python-reference.json', lambda d: epoch.canonical({**json.loads(d), 'generator': 'forged'})),
                         ('published/rust/ams_mel/index.html', lambda d: d + b'<!-- forged -->'),
                         ('native-anchors.json', lambda d: epoch.canonical({})),
                         ('published/api/index.html', lambda d: d.replace(b'role="separator"', b'role="button"')),
                         ('published/python/api-index.html', lambda d: d.replace(b'index.html', b'missing.html'))]
            for name, mutate in mutations:
                with self.subTest(path=name):
                    path = directory / name
                    path.write_bytes(mutate(original[name]))
                    # Re-seal the outer manifest: hashes alone must not pass.
                    # For Rust also re-seal its record to exercise regeneration.
                    if name.startswith('published/rust/'):
                        record = json.loads(original['rustdoc.json'])
                        record['files'][name.removeprefix('published/rust/')] = p.sha(path.read_bytes())
                        docs_ci.write(directory / 'rustdoc.json', record)
                    manifest = json.loads((directory / 'manifest.json').read_bytes())
                    payload = p.files(directory)
                    del payload['manifest.json']
                    manifest['payload'] = p.hashes(payload)
                    docs_ci.write(directory / 'manifest.json', manifest)
                    with self.assertRaises(ValueError):
                        parity.qualify(ROOT, directory, self.workspace / 'rejected.json',
                                       epoch.sha((directory / 'manifest.json').read_bytes()))
                    for restore in (name, 'manifest.json', 'rustdoc.json'):
                        (directory / restore).write_bytes(original[restore])
            for subtree in ('python', 'rust'):
                with self.subTest(missing_assembly=subtree):
                    shutil.rmtree(directory / 'published' / subtree)
                    manifest = json.loads(original['manifest.json'])
                    payload = p.files(directory)
                    del payload['manifest.json']
                    manifest['payload'] = p.hashes(payload)
                    docs_ci.write(directory / 'manifest.json', manifest)
                    with self.assertRaises(ValueError):
                        parity.qualify(ROOT, directory, self.workspace / 'rejected.json',
                                       epoch.sha((directory / 'manifest.json').read_bytes()))
                    shutil.copytree(self.candidate / 'published' / subtree, directory / 'published' / subtree)
                    (directory / 'manifest.json').write_bytes(original['manifest.json'])
        finally:
            shutil.rmtree(directory)

    def test_python_then_ada_rust_source_changes(self):
        repo = self.workspace / 'feature'
        subprocess.run(['git', 'clone', '-q', '--shared', '--no-checkout', str(ROOT), str(repo)], check=True)

        def git(*args):
            return subprocess.check_output(['git', '-C', str(repo), *args], text=True).strip()

        git('checkout', '-q', '--detach', self.result['identity']['commit'])
        git('config', 'user.name', 'Resize Regression Fixture')
        git('config', 'user.email', 'fixture@example.invalid')
        # Same failure shape as PY-MEL-018: a new public value produces a new
        # HTML page. Do not hardcode historical/global page counts.
        path = repo / 'python/ams_mel/common.py'
        text = path.read_text()
        self.assertEqual(text.count('__all__ = [\n'), 1)
        path.write_text(text.replace('__all__ = [\n', '__all__ = [\n    "ResizeQualificationValue",\n') +
                        '\nclass ResizeQualificationValue:\n'
                        '    """A documented value in a disposable source fixture."""\n'
                        '\n')
        path = repo / 'python/ams_mel/__init__.py'
        text = path.read_text()
        self.assertEqual(text.count('__all__ = [\n'), 1)
        path.write_text(text.replace('__all__ = [\n', '__all__ = [\n    "ResizeQualificationValue",\n') +
                        '\nfrom .common import ResizeQualificationValue\n')
        git('add', '-A')
        git('commit', '-qm', 'Python-only public documentation addition')
        publications = [self.actual]
        summaries = []
        for stage in ('python', 'ada-rust'):
            if stage == 'ada-rust':
                path = repo / 'ada/src/ams-mel-rf-product_rx.ads'
                text = path.read_text()
                self.assertEqual(text.count('\nprivate\n'), 1)
                path.write_text(text.replace('\nprivate\n', '\n   subtype Resize_Qualification_Count is Natural;\n'
                                '   --  Count values in a disposable documentation fixture.\n\nprivate\n'))
                path = repo / 'rust/ams-mel-common/src/lib.rs'
                path.write_text(path.read_text() + '\n/// A value in a disposable documentation fixture.\n'
                                'pub struct ResizeQualificationValue;\n')
                git('add', '-A')
                git('commit', '-qm', 'Independent documented Ada and Rust additions')
            directory = self.workspace / stage
            # Only the real child fixture identity is local. Production hosted
            # identity remains event-bound; never substitute the caller's HEAD.
            with mock.patch.dict(os.environ, {'GITHUB_ACTIONS': 'false'}):
                manifest = docs_ci.build(repo, directory, self.prefix)
                result = parity.qualify(repo, directory, self.workspace / f'{stage}-parity.json',
                                        epoch.sha((directory / 'manifest.json').read_bytes()))
            self.assertEqual(set(result['delta']), parity.DELTA)
            self.assertEqual(result['identity'], manifest['identity'])
            actual = p.files(directory / 'published')
            with self.assertRaisesRegex(ValueError, 'exact four-file'):
                parity.compare(publications[-1], actual)
            if stage == 'python':
                self.assertGreater(sum(n.startswith('python/') and n.endswith('.html') for n in actual),
                                   sum(n.startswith('python/') and n.endswith('.html') for n in publications[-1]))
                self.assertEqual({n: d for n, d in actual.items() if not n.startswith('python/')},
                                 {n: d for n, d in publications[-1].items() if not n.startswith('python/')})
            else:
                for prefix in ('api/', 'rust/'):
                    self.assertNotEqual({n: d for n, d in actual.items() if n.startswith(prefix)},
                                        {n: d for n, d in publications[-1].items() if n.startswith(prefix)})
            publications.append(actual)
            summaries.append(result)
        evidence = os.environ.get('MEL_RESIZE_REGRESSION_EVIDENCE')
        if evidence:
            Path(evidence).write_bytes(epoch.canonical(summaries))


if __name__ == '__main__':
    unittest.main()
