"""Consumer-owned source browser adversarial controls; current qualified inputs."""

import copy
import json
import os
from pathlib import Path
import re
import shutil
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
sys.path[:0] = [str(ROOT / 'docs/tools'), str(ROOT / 'scripts')]
import build_enhanced_site as p
import source_view_audit as source
import source_epoch as epoch
from source_view_parity import compare_releases
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
        p.require(json.loads(actual['api/presentation.json'])['source_view'] == manifest,
                  'source-view manifest differs from independently reconstructed evidence')
        return pages

    def reseal(self, actual):
        manifest = json.loads(actual['api/presentation.json'])
        manifest['output_sha256'] = p.hashes({n: b for n, b in actual.items()
                                             if n not in {'api/presentation.json', 'api/gnatdoc.css'}})
        actual['api/presentation.json'] = (json.dumps(manifest, sort_keys=True, indent=2, ensure_ascii=False) + '\n').encode()
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
                with self.assertRaisesRegex(ValueError, 'approved text/markup' if case == 'swap' else 'inventory mismatch'):
                    self.qualify(self.reseal(actual))

    def test_line_anchor_and_text_mutations_even_with_recomputed_hashes(self):
        name = next(iter(self.pages))
        original = self.pages[name]
        lines = list(re.finditer(rb'<span class="source-line".*?(?=<span class="source-line"|</code>)', original, re.S))
        first, second = lines[:2]
        for replacement in (original.replace(b'id="L1"', b'id="missing"', 1),
                            original.replace(b'id="L2"', b'id="L1"', 1),
                            original.replace(b'id="L2"', b'id="L3"', 1),
                            original[:first.start()] + original[first.end():],
                            original[:first.start()] + second[0] + first[0] + original[second.end():],
                            original.replace(b'ada-keyword">package', b'ada-keyword">forged', 1),
                            original.replace(b'</code>', b'<script>alert(1)</script></code>', 1),
                            original.replace(b'<body ', b'<body onload="alert(1)" ', 1),
                            original.replace(b'<body ', b'<body data-forged="1" ', 1),
                            original.replace(b'href="../../../gnatdoc-source.css"', b'href="https://evil.invalid/x"')):
            self.assertNotEqual(replacement, original)
            for reseal in (False, True):
                with self.subTest(reseal=reseal, mutation=p.sha(replacement)):
                    actual = dict(self.actual, **{name: replacement})
                    if reseal:
                        self.reseal(actual)
                    with self.assertRaisesRegex(ValueError, 'approved text/markup'):
                        self.qualify(actual)

    def test_token_mutations_and_stylesheet_even_with_recomputed_hashes(self):
        name, original = next(iter(self.pages.items()))
        token = source.TOKEN_SPAN.search(original)[0]
        cases = {'wrong-class': token.replace(b'ada-', b'unapproved-'),
                 'wrong-approved-class': re.sub(rb'ada-\w+', b'ada-string', token, count=1),
                 'active-attribute': token.replace(b'<span ', b'<span onclick="alert(1)" '),
                 'arbitrary-attribute': token.replace(b'<span ', b'<span data-added="1" '),
                 'unbalanced': token.removesuffix(b'</span>'),
                 'missing-wrapper': source.TOKEN_SPAN.sub(rb'\1', token),
                 'nested-wrapper': token.replace(b'</span>', b'<span class="ada-comment"></span></span>'),
                 'nested-resource': token.replace(b'</span>', b'<img src="https://example.invalid/x"></span>')}
        for case, changed in cases.items():
            self.assertNotEqual(changed, token)
            actual = dict(self.actual, **{name: original.replace(token, changed, 1)})
            with self.subTest(case=case), self.assertRaisesRegex(ValueError, 'approved text/markup'):
                self.qualify(self.reseal(actual))
        actual = dict(self.actual, **{source.CSS: self.actual[source.CSS] + b'\n/* changed */\n'})
        with self.assertRaisesRegex(ValueError, 'stylesheet identity mismatch'):
            self.qualify(self.reseal(actual))

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
            self.assertNotEqual(text, original)
            with self.assertRaisesRegex(ValueError, 'declaration source destination'):
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
                files[0]['declarations'][0]['line'] += 1
            else:
                files[0]['source'] = {'private': 'ada/src/ams-mel_c_api.ads',
                                     'traversal': 'ada/src/../../secret.ads',
                                     'encoded': 'ada/src/%2e%2e/secret.ads'}[case]
            actual['api/presentation.json'] = json.dumps(manifest).encode()
            with self.subTest(case=case), self.assertRaisesRegex(ValueError, 'source-view manifest differs'):
                self.qualify(actual)

    def test_placeholder_and_forged_native_evidence_cannot_authorize_publication(self):
        from ada_gnatdoc_enhancer.source_view import native_evidence
        from ada_gnatdoc_enhancer.errors import EnhancementError

        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            shutil.copytree(ROOT / 'ada/src', root / 'ada/src')
            (root / 'docs').mkdir()
            shutil.copyfile(ROOT / source.CONFIG, root / source.CONFIG)
            expected = source.evidence(root, self.site)
            configured = source.options(root, self.site)
            path = root / source.EVIDENCE
            path.write_bytes(epoch.canonical(expected))
            with self.assertRaisesRegex(EnhancementError, 'evidence hash mismatch'):
                native_evidence(root, dict(configured, native_manifest_sha256='0' * 64))
            self.assertEqual(source.prepare(root, self.site), 'docs/gnatdoc-effective.toml')
            self.assertEqual(native_evidence(root, configured), expected)
            for key in ('files', 'sources'):
                forged = copy.deepcopy(expected)
                forged[key][next(iter(forged[key]))] = 'a' * 64
                path.write_bytes(epoch.canonical(forged))
                # Reseal the output-side evidence claim as well. Neither claim
                # alters the consumer's reconstruction from source and native.
                with self.subTest(key=key), self.assertRaisesRegex(ValueError, 'source/native evidence changed'):
                    source.prepare(root, self.site)
                actual = dict(self.actual)
                manifest = json.loads(actual['api/presentation.json'])
                manifest['source_view']['native_evidence']['native_manifest_sha256'] = p.sha(path.read_bytes())
                actual['api/presentation.json'] = json.dumps(manifest).encode()
                with self.assertRaisesRegex(ValueError, 'source-view manifest differs'):
                    self.qualify(self.reseal(actual))

    def test_source_input_symlinks_rejected(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            (root / 'ada').mkdir()
            (root / 'ada/src').symlink_to(ROOT / 'ada/src', target_is_directory=True)
            with self.assertRaisesRegex(ValueError, 'symlink'):
                source.sources(root)

    def test_source_paths_and_private_or_body_inputs_cannot_authorize_publication(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            directory = root / 'ada/src'
            directory.mkdir(parents=True)
            (directory / 'safe.ads').write_text('package Safe is\nend Safe;\n')
            (directory / 'hidden.ads').write_text('private package Hidden is\nend Hidden;\n')
            (directory / 'safe.adb').write_text('package body Safe is\nend Safe;\n')
            self.assertEqual(set(source.source_pages(root)), {'api/source/ada/src/safe.ads.html'})
            for name in ('bad%2fpath.ads', 'bad name.ads'):
                path = directory / name
                path.write_text('package Unsafe is\nend Unsafe;\n')
                with self.subTest(name=name), self.assertRaises(ValueError):
                    source.source_pages(root)
                path.unlink()
            path = directory / 'safe.ads'
            path.unlink()
            path.symlink_to(ROOT / 'ada/src/ams.ads')
            with self.assertRaisesRegex(ValueError, 'symlink'):
                source.source_pages(root)

    def test_native_source_insertions_are_exact_and_cannot_add_attributes(self):
        name, record = next(iter(self.insertions.items()))
        original = self.actual[name].decode()
        for text in (original.replace('class="view-source"', 'class="view-source" data-forged="1"', 1),
                     original.replace('class="view-source"', 'class="view-source" onclick="alert(1)"', 1),
                     original.replace(source.HEAD, source.HEAD + '<script>alert(1)</script>', 1),
                     original.replace(source.HEAD, source.HEAD + source.HEAD, 1),
                     original.replace(source.HEAD, '', 1) + source.HEAD,
                     original.replace('href="source/', 'href="https://evil.invalid/source/', 1),
                     original.replace('href="source/', 'href="source/../', 1),
                     original.replace('href="source/', 'href="source/%2e%2e/', 1)):
            with self.assertRaises(ValueError):
                self.qualify(self.reseal(dict(self.actual, **{name: text.encode()})))

    def test_malicious_comments_are_only_exact_escaped_source_text(self):
        from check_live_docs_site import GraphPage
        raw = b'package Safe is\n-- <script>alert(1)</script> {{ /home/user file:///tmp/x\nend Safe;\n'
        rendered, lines = source.page('ada/src/safe.ads', raw)
        self.assertEqual(lines, 3)
        self.assertNotIn(b'<script>', rendered)
        self.assertIn(b'<span class="ada-comment">-- &lt;script&gt;', rendered)
        self.assertIn(b'&#123;&#123;', rendered)
        self.assertFalse(p.Page(rendered.decode()).issues)
        GraphPage(rendered.decode(), 'api/source/ada/src/safe.ads.html')

    def test_highlighting_preserves_exact_line_format_and_every_token_kind(self):
        raw = (b'package Safe is\r\n   C : Character := Character\'(\'A\');\r\n'
               b'   S : String := "quoted ""text""";\n   N : Integer := 16#FF#; -- < >\n'
               b"   B : Boolean := N = Integer'Last;\nend Safe;")
        name = 'ada/src/safe.ads'
        highlighted, count = source.page(name, raw)
        plain, _ = source.page(name, raw, highlighted=False)
        self.assertEqual(count, 6)
        self.assertEqual(source.TOKEN_SPAN.sub(rb'\1', highlighted), plain)
        self.assertEqual(set(re.findall(rb'class="ada-(\w+)"', highlighted)),
                         {kind.encode() for kind in source.TOKEN_CLASSES})
        from html import unescape
        code = re.search(rb'<code>(.*?)</code>', highlighted, re.S)[1]
        self.assertEqual(unescape(re.sub(rb'<[^>]+>', b'', code).decode()).encode(), raw)


class CompatibilityMutationTests(unittest.TestCase):
    def test_disabled_delta_allows_only_the_exact_serialized_version(self):
        name = 'api/presentation.json'
        old = {name: b'{\n  "enhancer_version": "0.1.7"\n}\n', 'rust/index.html': b'unchanged'}
        new = dict(old, **{name: old[name].replace(b'0.1.7', b'0.1.8')})
        self.assertEqual(compare_releases(old, new)['delta'], [name])
        cases = [('noncanonical serialized', dict(new, **{name: new[name].rstrip()})),
                 ('unrelated release presentation metadata',
                  dict(new, **{name: b'{\n  "counts": {},\n  "enhancer_version": "0.1.8"\n}\n'})),
                 ('exceeds approved files', dict(new, **{'rust/index.html': b'changed'}))]
        for reason, mutant in cases:
            with self.subTest(reason=reason), self.assertRaisesRegex(ValueError, reason):
                compare_releases(old, mutant)


if __name__ == '__main__':
    unittest.main()
