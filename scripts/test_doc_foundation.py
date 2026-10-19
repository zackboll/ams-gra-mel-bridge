"""Unit and mutation qualification for the pristine MEL documentation site."""

import copy
import json
import shutil
import tempfile
import unittest
from pathlib import Path

from check_doc_coverage import (CoverageError, ROOT, measure, ratchet, meaningful,
                              verified_exceptions, source_prose)
from check_doc_source_integrity import tokens, native_profile
from check_doc_baseline_delta import delta
from classify_gnatdoc_warnings import classify
from compare_gnatdoc_warnings import WarningComparisonError
from gnatdoc_inventory import specifications
from gnatdoc_coverage_model import specification
from validate_docs import validate, SiteError


class SourceTests(unittest.TestCase):
    def test_literals_and_comments(self):
        self.assertEqual(tokens('X := "--"; -- comment\nY := \'a\';'),
                         ['X', ':=', '"--"', ';', 'Y', ':=', "'a'", ';'])
        self.assertNotEqual(tokens('X := "a b";'), tokens('X := "a  b";'))
        self.assertNotEqual(tokens('X : T := 1;'), tokens('X : T := 2;'))
        self.assertNotEqual(tokens('X : T;'), tokens('X:T1;'))

    def test_full_profile_keeps_default_literal(self):
        self.assertNotEqual(native_profile('function F (S : String := "a") return T;'),
                            native_profile('function F (S : String := "b") return T;'))
        self.assertEqual(native_profile('procedure F (X : T) renames G;'),
                         native_profile('procedure F (X : T)'))

    def test_empty_tags_and_identifier_repetitions_not_meaningful(self):
        self.assertFalse(meaningful('Value'))
        self.assertFalse(meaningful(''))
        self.assertEqual(source_prose(['@param Value', '@return'], 'param', 'Value'), '')


class WarningClassificationTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.src = self.root / 'ada/src'
        self.src.mkdir(parents=True)
        self.lines = ['package Demo is', '   type Counts is record',
                      '      First,', '      Second : Natural;', '   end record;',
                      '   function F (X : Integer) return Integer;', 'private',
                      '   function F (X : Float) return Integer;', 'end Demo;']
        (self.src / 'demo.ads').write_text('\n'.join(self.lines))
        (self.src / 'demo-hidden.ads').write_text(
            'private package Demo.Hidden is\n   Value : Integer;\nend Demo.Hidden;\n')

    def test_scope_and_multiline_component_classification(self):
        text = ('demo.ads:2:9: warning: component `First` is not documented\n'
                'demo.ads:6:13: warning: parameter `X` is not documented\n'
                'demo.ads:8:13: warning: parameter `X` is not documented\n'
                'demo-hidden.ads:2:4: warning: entity `Value` is not documented\n')
        report = classify(text, self.root)
        self.assertEqual(report['raw_total'], 4)
        self.assertEqual(report['counts'], {'public': 2, 'private-part': 1,
                                          'private-unit': 1, 'tool': 0, 'unclassified': 0})
        self.assertEqual(report['warnings'][0]['scope'], 'Demo.Counts.First')
        self.assertIn('Float', report['warnings'][2]['identity'])

    def test_line_shifts_keep_identity(self):
        before = classify('demo.ads:6:13: warning: return value is not documented\n', self.root)
        (self.src / 'demo.ads').write_text('-- inserted\n\n' + '\n'.join(self.lines))
        after = classify('demo.ads:8:13: warning: return value is not documented\n', self.root)
        self.assertEqual(before['warnings'], after['warnings'])

    def test_malformed_and_unmapped_fail(self):
        for text in ('warning: unknown\n', 'demo.ads:6:2: warning: entity `F` is not documented\n',
                     'missing.ads:1:1: warning: entity `F` is not documented\n',
                     'demo.ads:6:13: warning: parameter `Wrong` is not documented\n',
                     'demo.ads:2:9: warning: component `Absent` is not documented\n'):
            with self.subTest(text=text), self.assertRaises(WarningComparisonError):
                classify(text, self.root)


class ExceptionTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        (self.root / 'docs').mkdir()
        self.data = json.loads((ROOT / 'docs/gnatdoc-renderer-exceptions.json').read_text())
        self.declarations = [(name, self.root / path.relative_to(ROOT), d)
                             for name, path in specifications()[0].items()
                             for d in specification(path)[4]]

    def check(self, data, declarations=None):
        (self.root / 'docs/gnatdoc-renderer-exceptions.json').write_text(json.dumps(data))
        return verified_exceptions(self.root, self.declarations if declarations is None else declarations)

    def test_exact_alias(self):
        self.assertEqual(len(self.check(self.data)), 1)

    def test_wrong_target_renamed_source_duplicate_and_version(self):
        for field, value in (('target', 'Other'), ('identity', 'Other'),
                             ('source', 'ada/src/renamed.ads')):
            data = copy.deepcopy(self.data)
            data['exceptions'][0][field] = value
            with self.subTest(field=field), self.assertRaises(CoverageError):
                self.check(data)
        data = copy.deepcopy(self.data)
        data['exceptions'] *= 2
        with self.assertRaises(CoverageError):
            self.check(data)
        data['renderer'] = 'GNATdoc 27'
        with self.assertRaises(CoverageError):
            self.check(data)

    def test_missing_real_declaration(self):
        with self.assertRaises(CoverageError):
            self.check(self.data, [])


class RatchetTests(unittest.TestCase):
    def test_baseline_rewrite_cannot_authorize_new_gap(self):
        self.assertEqual(delta('old\n', 'old\nnew\n'), ['new'])
        self.assertEqual(delta('old\nfixed\n', 'old\n'), [])
        self.assertEqual(delta('old\n', 'old\nold\n'), ['old'])

    def test_add_remove_duplicate_and_unchanged(self):
        report = {'elements': [{'identity': 'Demo: F', 'documented': False}]}
        ratchet(report, 'Demo: F\n')
        for baseline in ('', 'Demo: F\nDemo: F\n', 'Demo: G\n', 'Demo: F\nDemo: G\n'):
            with self.subTest(baseline=baseline), self.assertRaises(CoverageError):
                ratchet(report, baseline)
        report['elements'][0]['documented'] = True
        with self.assertRaises(CoverageError):
            ratchet(report, 'Demo: F\n')


class SiteMutations(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        if not (ROOT / '_site/index.html').exists():
            raise RuntimeError('generate the pristine site before mutation qualification; do not skip')
        cls.manifest = json.loads((ROOT / 'docs/gnatdoc-native-anchors.json').read_text())

    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.site = Path(self.temp.name) / 'site'
        shutil.copytree(ROOT / '_site', self.site)

    def test_valid_site_and_coverage(self):
        self.assertEqual(validate(self.site)['html_pages'], 38)
        self.assertEqual(len(list((self.site / 'api').glob('*.html'))), 30)
        report = measure(self.site, anchor_manifest=self.manifest)
        self.assertEqual(report['source_declarations'], report['native_sections'] + 1)
        ratchet(report, (ROOT / 'docs/gnatdoc-coverage-baseline.txt').read_text())

    def mutate_page(self, change):
        entry = next(iter(self.manifest.values()))
        path = self.site / 'api' / entry['page']
        path.write_text(change(path.read_text(), entry))

    def test_missing_real_declaration(self):
        self.mutate_page(lambda s, e: s.replace('<h4 id=' + e['anchor'], '<h3 id=' + e['anchor'], 1))
        with self.assertRaises(CoverageError):
            measure(self.site)

    def test_duplicate_symbol(self):
        import re
        self.mutate_page(lambda s, e: s + re.search(r'<h4 .*?</code></pre>', s, re.S)[0])
        with self.assertRaises(CoverageError):
            measure(self.site)

    def test_changed_anchor(self):
        self.mutate_page(lambda s, e: s.replace(e['anchor'], '0' * 64))
        with self.assertRaises(CoverageError):
            measure(self.site, anchor_manifest=self.manifest)

    def test_swapped_overload_anchors(self):
        import re
        for path in (self.site / 'api').glob('*.html'):
            matches = re.findall(r'<h4 id=([^ >]+)>([^<]+)</h4>', path.read_text())
            pair = next(((a, b) for a in matches for b in matches
                         if a[1] == b[1] and a[0] != b[0]), None)
            if pair:
                text = path.read_text().replace(pair[0][0], 'TEMPANCHOR')
                text = text.replace(pair[1][0], pair[0][0]).replace('TEMPANCHOR', pair[1][0])
                path.write_text(text)
                with self.assertRaises(CoverageError):
                    measure(self.site, anchor_manifest=self.manifest)
                return
        self.fail('expected overloaded source profiles in native output')

    def test_missing_member_is_not_silent_gap(self):
        for path in (self.site / 'api').glob('*.html'):
            text = path.read_text()
            if '<h5>Parameters</h5>' in text:
                path.write_text(text.replace('<h5>Parameters</h5>', '<h5>Lost</h5>', 1))
                with self.assertRaises(CoverageError):
                    measure(self.site)
                return
        self.fail('expected parameters')

    def test_signature_default_literal_mutation(self):
        import re
        for path in (self.site / 'api').glob('*.html'):
            text = path.read_text()
            # Modify actual native code, not a description or source identity.
            match = re.search(r'Config\s+: String := ""', text)
            if match:
                path.write_text(text[:match.start()] + match[0].replace('""', '"changed"')
                                + text[match.end():])
                with self.assertRaises(CoverageError):
                    measure(self.site)
                return
        self.fail('expected string default in native API profile')

    def test_swapped_package(self):
        self.mutate_page(lambda s, e: s.replace('<h1>', '<h1>Wrong.', 1))
        with self.assertRaises(CoverageError):
            measure(self.site)

    def test_unexpected_omission(self):
        (self.site / 'api' / next(iter(self.manifest.values()))['page']).unlink()
        with self.assertRaises(CoverageError):
            measure(self.site)

    def test_corrupted_documentation_cannot_count(self):
        report = measure(self.site)
        self.mutate_page(lambda s, e: s.replace('<h2>Description</h2>',
                        '<h2>Description</h2><p>Fabricated unsupported coverage.'))
        altered = measure(self.site)
        self.assertEqual(report['documented_elements'], altered['documented_elements'])

    def test_unsafe_resources_links_and_missing_assets(self):
        landing = self.site / 'index.html'
        original = landing.read_text()
        for mutation in ('<script>alert(1)</script>', '<a href="../outside">bad</a>',
                         '<img src="https://example.org/x">', '<p onclick="x()">bad</p>',
                         '<a href="api/index.html#absent">bad</a>',
                         '<a href="missing.html">bad</a>', '<p id="x"><p id="x">'):
            landing.write_text(original + mutation)
            with self.subTest(mutation=mutation), self.assertRaises(SiteError):
                validate(self.site)
        landing.write_text(original)
        (self.site / 'api/gnatdoc.css').unlink()
        with self.assertRaises(SiteError):
            validate(self.site)

    def test_swapped_navigation_destinations(self):
        import re
        path = self.site / 'api/index.html'
        text = path.read_text()
        destinations = re.findall(r'<a href=([^ >]+) target=document-content>', text)
        text = text.replace(destinations[0], 'TEMPDEST').replace(destinations[1], destinations[0])
        path.write_text(text.replace('TEMPDEST', destinations[1]))
        with self.assertRaises(SiteError):
            validate(self.site)

    def test_corrupted_summary_fails_coverage_ratchet(self):
        import re
        baseline = (ROOT / 'docs/gnatdoc-coverage-baseline.txt').read_text()
        before = measure(self.site)
        for element in before['elements']:
            if element['documented'] and element['kind'] in ('function', 'procedure'):
                identity = element['identity'].removesuffix(': description')
                native = before['native'][identity]
                path = self.site / 'api' / native['page']
                text = path.read_text()
                pattern = r'(<h4 id=' + native['anchor'] + r'>.*?</code></pre>)(.*?)(?=<h4 |\Z)'
                match = re.search(pattern, text, re.S)
                body = re.sub(r'^.*?(?=<h5>|\Z)', '<p>Corrupted summary.', match[2], count=1, flags=re.S)
                path.write_text(text[:match.start(2)] + body + text[match.end(2):])
                with self.assertRaises(CoverageError):
                    ratchet(measure(self.site), baseline)
                return
        self.fail('expected documented operation summary')


if __name__ == '__main__':
    unittest.main()
