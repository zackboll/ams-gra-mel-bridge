"""Consumer-owned source browser adversarial controls; current qualified inputs."""

import copy
import json
import os
from pathlib import Path
import re
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
sys.path[:0] = [str(ROOT / 'docs/tools'), str(ROOT / 'scripts')]
import build_enhanced_site as p
import source_view_audit as source
import source_epoch as epoch
from local_type_audit import Authority
from check_doc_coverage import measure


class SourcePublicationTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.directory = Path(os.environ.get('MEL_SOURCE_QUALIFICATION', ROOT / '.doc-tools/source-qualified'))
        cls.site = cls.directory / 'pristine'
        cls.report = measure(cls.site, ROOT)
        cls.authority = Authority(ROOT, cls.site, ledger=cls.report['native'])
        cls.actual = p.files(cls.directory / 'ada-qualified')
        cls.pages, cls.insertions, cls.manifest = source.reconstruct(ROOT, cls.site, cls.authority)

    def qualify(self, actual):
        pages, insertions, manifest = source.qualify(ROOT, self.site, self.authority, actual)
        for name, record in insertions.items():
            source.restore(actual[name].decode(), (self.site / name).read_text(), record)
        self.assertEqual(json.loads(actual['api/presentation.json'])['source_view'], manifest)
        return pages

    def reseal(self, actual):
        manifest = json.loads(actual['api/presentation.json'])
        manifest['output_sha256'].update(p.hashes({n: b for n, b in actual.items()
                                                  if n != 'api/presentation.json'}))
        actual['api/presentation.json'] = json.dumps(manifest).encode()
        return actual

    def test_complete_source_and_destination_contract(self):
        self.assertEqual(self.qualify(self.actual), self.pages)

    def test_missing_extra_duplicate_and_reordered_source_pages(self):
        first, second = list(self.pages)[:2]
        for case in ('missing', 'extra', 'duplicate', 'swap'):
            with self.subTest(case=case):
                actual = dict(self.actual)
                if case == 'missing':
                    del actual[first]
                elif case in ('extra', 'duplicate'):
                    actual[first.replace('.ads.html', '.adb.html' if case == 'extra' else '-copy.ads.html')] = actual[first]
                else:
                    actual[first], actual[second] = actual[second], actual[first]
                with self.assertRaises(ValueError):
                    self.qualify(self.reseal(actual))

    def test_line_anchor_and_text_mutations_even_with_recomputed_hashes(self):
        name = next(iter(self.pages))
        original = self.pages[name]
        for replacement in (original.replace(b'id="L1"', b'id="missing"', 1),
                            original.replace(b'id="L2"', b'id="L1"', 1),
                            original.replace(b'id="L2"', b'id="L3"', 1),
                            original.replace(b'package ', b'forged ', 1),
                            original.replace(b'</code>', b'<script>alert(1)</script></code>', 1),
                            original.replace(b'<body ', b'<body onload="alert(1)" ', 1),
                            original.replace(b'<body ', b'<body data-forged="1" ', 1),
                            original.replace(b'href="../../../gnatdoc-source.css"', b'href="https://evil.invalid/x"')):
            if replacement == original:
                continue
            for reseal in (False, True):
                with self.subTest(reseal=reseal, mutation=p.sha(replacement)):
                    actual = dict(self.actual, **{name: replacement})
                    if reseal:
                        self.reseal(actual)
                    with self.assertRaises(ValueError):
                        self.qualify(actual)

    def test_wrong_existing_line_and_overload_swaps(self):
        for name, record in self.insertions.items():
            groups = {}
            for entry in record[2][1:]:
                groups.setdefault(entry['name'], []).append(entry)
            overloads = next((g for g in groups.values() if len(g) > 1), None)
            if overloads:
                break
        self.assertIsNotNone(overloads)
        first, second = overloads[:2]
        original = self.actual[name].decode()
        a, b = '.html#L' + str(first['line']) + '"', '.html#L' + str(second['line']) + '"'
        wrong = original.replace(a, '.html#L1"', 1)
        swapped = original.replace(a, 'SWAP', 1).replace(b, a, 1).replace('SWAP', b, 1)
        for text in (wrong, swapped):
            with self.assertRaises(ValueError):
                self.qualify(self.reseal(dict(self.actual, **{name: text.encode()})))

    def test_forged_ownership_order_hash_and_manifest_destinations(self):
        for case in ('ownership', 'order', 'duplicate', 'hash', 'line', 'private', 'traversal', 'encoded'):
            actual = dict(self.actual)
            manifest = json.loads(actual['api/presentation.json'])
            files = manifest['source_view']['files']
            if case == 'ownership':
                files[0]['package'] = files[1]['package']
            elif case == 'order':
                files.reverse()
            elif case == 'duplicate':
                files.append(copy.deepcopy(files[0]))
            elif case == 'hash':
                files[0]['sha256'] = '0' * 64
            elif case == 'line':
                files[0]['declarations'][0]['line'] = 1
            else:
                files[0]['source'] = {'private': 'ada/src/ams-mel_c_api.ads',
                                     'traversal': 'ada/src/../../secret.ads',
                                     'encoded': 'ada/src/%2e%2e/secret.ads'}[case]
            actual['api/presentation.json'] = json.dumps(manifest).encode()
            with self.subTest(case=case), self.assertRaises((ValueError, AssertionError)):
                self.qualify(actual)

    def test_source_input_symlinks_rejected(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            (root / 'ada').mkdir()
            (root / 'ada/src').symlink_to(ROOT / 'ada/src', target_is_directory=True)
            with self.assertRaisesRegex(ValueError, 'symlink'):
                source.sources(root)

    def test_malicious_comments_are_only_exact_escaped_source_text(self):
        from check_live_docs_site import GraphPage
        raw = b'package Safe is\n-- <script>alert(1)</script> {{ /home/user file:///tmp/x\nend Safe;\n'
        rendered, lines = source.page('ada/src/safe.ads', raw)
        self.assertEqual(lines, 3)
        self.assertNotIn(b'<script>', rendered)
        self.assertIn(b'&lt;script&gt;', rendered)
        self.assertIn(b'&#123;&#123;', rendered)
        self.assertFalse(p.Page(rendered.decode()).issues)
        GraphPage(rendered.decode(), 'api/source/ada/src/safe.ads.html')


if __name__ == '__main__':
    unittest.main()