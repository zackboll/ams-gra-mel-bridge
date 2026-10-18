"""Exercise semantic protection directly, without package/manifest reconstruction."""

import json
import re
import sys
import tempfile
import unittest
from dataclasses import replace
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'docs/tools'))
sys.path.insert(0, str(ROOT / 'scripts'))
import build_enhanced_site as presentation
from local_type_audit import Authority, LINK, SECTION, restore_page, lex, type_context
from ada_gnatdoc_enhancer.config import load
from ada_gnatdoc_enhancer.pipeline import expected_output
from ada_gnatdoc_enhancer.syntax import linked


class IndependentTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.authority = Authority(ROOT, ROOT / '_site')
        _, cls.output, _ = expected_output(load(ROOT, 'docs/gnatdoc-enhance.toml'), ROOT / '_site')
        cls.records = []
        for name, data in cls.output.items():
            if name.endswith('.html'):
                _, records = restore_page(Path(name).name, data.decode(), cls.authority)
                cls.records.extend(records)
        cls.example = next(r for r in cls.records if r['visible'] == 'Complex_I16')
        cls.page = cls.example['containing']['page']
        cls.text = cls.output['api/' + cls.page].decode()
        cls.markup = next(m[0] for m in LINK.finditer(cls.text) if m[3] == 'Complex_I16')

    def reject(self, text):
        with self.assertRaisesRegex(ValueError, 'independent type audit'):
            restore_page(self.page, text, self.authority)

    def test_complete_independent_occurrence_count(self):
        self.assertEqual(len(self.records), 1418)
        self.assertTrue(all(r['target']['identity'].split(': ')[2] in ('type', 'subtype') for r in self.records))

    def test_real_same_package_array_pairs(self):
        for name in ('Complex_I16', 'Complex_I8'):
            records = [r for r in self.records if r['visible'] == name]
            self.assertEqual(len(records), 1)
            record = records[0]
            self.assertEqual(record['context'], 'array-element')
            self.assertEqual(record['containing']['name'], name + '_Array')
            self.assertEqual(record['target']['name'], name)
            self.assertIn('record', record['target']['native'])

    def test_real_qualified_euler_components_and_private_body_parameter(self):
        records = [r for r in self.records if r['visible'] == 'AMS.MEL.Status.Euler'
                   and r['containing']['package'] == 'ams.mel.ir.image']
        self.assertTrue(any('Euler_Value' in r['containing']['declaration'] for r in records))
        self.assertTrue(any(re.search(r'Value\s*:\s*AMS.MEL.Status.Euler', r['containing']['declaration'])
                            and r['containing']['name'] == 'Attitude_Rate' for r in records))
        self.assertTrue(all(r['target_identity'] == 'ams.mel.status.euler' for r in records))
        body = (ROOT / 'ada/src/ams-mel-ir-image.adb').read_text()
        self.assertIn('function Raw_Euler (Value : AMS.MEL.Status.Euler)', body)
        self.assertFalse(any(e['name'] == 'Raw_Euler' for e in self.authority.sections.values()))

    def test_real_ancestor_and_current_visibility(self):
        self.assertTrue(any(r['containing']['package'] == r['target']['package'] for r in self.records))
        self.assertTrue(any('.' not in r['visible'] and r['containing']['package'].startswith(r['target']['package'] + '.')
                            for r in self.records))

    def test_real_alias_returns_remain_plain(self):
        page = next(e['page'] for e in self.authority.sections.values()
                    if e['package'] == 'ams.mel.ir.health_status.metadata')
        text = self.output['api/' + page].decode()
        restored, _ = restore_page(page, text, self.authority)
        for spelling in ('Status.MFA_Status', 'Status.BIT_Status', 'Status.Discrete_Status',
                         'Status.Security_Audit_Record', 'Status.MFA_Status_Detailed'):
            self.assertIn(spelling, restored)
            self.assertFalse(any(m[3] == spelling for m in LINK.finditer(text)))
        self.assertNotIn('package Status renames', text)

    def test_complete_restoration_including_original_hyperlinks(self):
        for name, data in self.output.items():
            if name.endswith('.html'):
                restored, _ = restore_page(Path(name).name, data.decode(), self.authority)
                restored = restored.replace(presentation.METADATA, '', 1)
                self.assertEqual(restored.encode(), (ROOT / '_site' / name).read_bytes(), name)

    def test_wrong_existing_destination_and_converse(self):
        target = next(r for r in self.records if r['target_identity'] != self.example['target_identity'])
        for forged in (self.markup.replace(self.example['href'], target['href']),
                       self.markup.replace(self.example['target_identity'], target['target_identity'])):
            self.reject(self.text.replace(self.markup, forged, 1))

    def test_forged_consistent_destination_identity_pair(self):
        target = next(r for r in self.records if r['target_identity'] != self.example['target_identity'])
        forged = self.markup.replace(self.example['href'], target['href']).replace(
            self.example['target_identity'], target['target_identity'])
        self.reject(self.text.replace(self.markup, forged, 1))

    def test_changed_visible_text(self):
        self.reject(self.text.replace(self.markup, self.markup.replace('>Complex_I16<', '>Complex_I8<'), 1))

    def test_duplicate_nested_and_active_markup(self):
        for forged in (self.markup * 2, self.markup.replace('>Complex_I16<', '>' + self.markup + '<'),
                       self.markup.replace(' href=', ' onclick="run()" href='),
                       self.markup.replace(' href=', ' href="unexpected" href='),
                       self.markup.replace('>Complex_I16<', '><span class="ada-keyword">Complex_I16</span><')):
            self.reject(self.text.replace(self.markup, forged, 1))

    def test_external_traversal_and_malformed_markup(self):
        for href in ('https://example.org/x#y', '../' + self.example['href'],
                     '/' + self.example['href'], 'javascript:alert(1)'):
            self.reject(self.text.replace(self.markup, self.markup.replace(self.example['href'], href), 1))
        self.reject(self.text.replace(self.markup, self.markup.replace('</a>', '</span>'), 1))

    def test_moved_to_defining_name(self):
        # Preserve visible bytes but move the anchor from the array element to
        # its defining name. This fails semantic location, not an output hash.
        section = next(m for m in SECTION.finditer(self.text) if self.markup in m[3])
        body = section[3].replace(self.markup, 'Complex_I16', 1)
        body = body.replace('Complex_I16_Array', self.markup.replace('>Complex_I16<', '>Complex_I16_Array<'), 1)
        self.reject(self.text.replace(section[3], body, 1))

    def test_context_literals_comments_defining_names_and_attributes(self):
        cases = [('procedure F (S : String := "return T");', 'T', False),
                 ('procedure F; -- return T', 'T', False),
                 ('type T is private;', 'T', False),
                 ("procedure F (X : T'Class);", "'Class", False),
                 ("procedure F (X : T'Class);", 'T', True),
                 ('function F (X : Integer := G) return T;', 'T', True),
                 ('function F return Boolean with Post => Return T;', 'T', False),
                 ('X : Boolean := T;', 'T', False)]
        for text, token, accepted in cases:
            with self.subTest(text=text, token=token):
                start = text.index(token)
                tokens = lex(text)
                indices = [i for i, t in enumerate(tokens) if t[1] == start]
                if accepted:
                    type_context(tokens, indices[0], start + len(token))
                else:
                    with self.assertRaises((ValueError, IndexError)):
                        type_context(tokens, indices[0], start + len(token))

    def test_ambiguity_shadowing_unknown_alias_controls(self):
        a = Authority.__new__(Authority)
        a.types = {'p.t': {}, 'q.t': {}, 'parent.t': {}}
        a.packages = {'p': {'declarations': [], 'imports': {'p', 'q'}, 'uses': {'p', 'q'}},
                      'parent.child': {'declarations': [{'name': 'T'}], 'imports': set(), 'uses': set()}}
        for spelling, owner in [('T', 'parent.child'), ('Unknown', 'p'), ('Alias.T', 'p')]:
            with self.assertRaises(ValueError):
                a.destination(spelling, owner)
        a.packages['consumer'] = {'declarations': [], 'imports': {'p', 'q'}, 'uses': {'p', 'q'}}
        with self.assertRaisesRegex(ValueError, 'ambiguous'):
            a.destination('T', 'consumer')

    def test_markup_in_non_type_contexts_directly(self):
        # Minimal authority fixtures let markup reach semantic validation. No
        # package fingerprint or enhancer-output mismatch can explain rejection.
        page, anchor = 'a' * 64 + '.html', 'b' * 64
        href = page + '#' + 'c' * 64
        cases = [('procedure F (X : T);', 'T', True),
                 ('procedure F (S : String := "return T");', 'T', False),
                 ('procedure F; -- return T', 'T', False),
                 ('type T is private;', 'T', False),
                 ("procedure F (X : T'Class);", "'Class", False),
                 ("procedure F (X : T'Class);", 'T', True),
                 ('X : Boolean := T;', 'T', False)]
        for native, token, accepted in cases:
            with self.subTest(native=native, token=token):
                authority = Authority.__new__(Authority)
                entry = {'native': native, 'declaration': native, 'package': 'p', 'line': 1}
                authority.sections = {(page, anchor): entry}
                authority.types = {'p.t': {'page': page, 'anchor': 'c' * 64}}
                authority.packages = {'p': {'declarations': [{'name': 'T'}], 'imports': set(), 'uses': set()}}
                start = native.index(token)
                wrapper = f'<a class="type-reference" href="{href}" data-type="p.t">{token}</a>'
                marked = native[:start] + wrapper + native[start + len(token):]
                snippet = f'<h4 id={anchor}>F</h4><pre class=ada-code-snippet><code>{marked}</code></pre>'
                if accepted:
                    _, records = restore_page(page, snippet, authority)
                    self.assertEqual(len(records), 1)
                else:
                    with self.assertRaisesRegex(ValueError, 'independent type audit'):
                        restore_page(page, snippet, authority)

    def test_shared_ambiguity_and_shadowing_stay_plain(self):
        from ada_gnatdoc_enhancer.inventory import Package, Declaration
        config = load(ROOT, 'docs/gnatdoc-enhance.toml')
        targets = {'p.t': (Path('observed.html'), 't'), 'q.t': (Path('other.html'), 't')}
        ambiguous = Package('Consumer', Path('consumer.ads'), frozenset({'p', 'q'}), frozenset({'p', 'q'}), {}, [], '')
        shadowed = Package('P.Child', Path('child.ads'), frozenset(), frozenset(), {},
                           [Declaration('function', 'T', 'function T return Boolean;', 1)], '')
        for pkg in (ambiguous, shadowed):
            raw = 'procedure F (Value : T);'
            result, count = linked(raw, pkg, {pkg.name.lower(): pkg}, None, targets, config)
            self.assertEqual((result, count), (raw, 0))

    def test_wrong_target_with_internally_consistent_manifest(self):
        target = next(r for r in self.records if r['target_identity'] != self.example['target_identity'])
        with tempfile.TemporaryDirectory() as temp:
            site = Path(temp) / 'site'
            presentation.build(ROOT / '_site', site)
            page = site / 'api' / self.page
            forged = self.markup.replace(self.example['href'], target['href']).replace(
                self.example['target_identity'], target['target_identity'])
            page.write_text(page.read_text().replace(self.markup, forged, 1))
            manifest = site / 'api/presentation.json'
            data = json.loads(manifest.read_text())
            data['output_sha256']['api/' + self.page] = presentation.sha(page.read_bytes())
            manifest.write_text(json.dumps(data))
            with self.assertRaisesRegex(ValueError, 'independent type audit'):
                presentation.validate(site, ROOT / '_site')

    def test_literal_comment_regression_selected_implementation(self):
        from ada_gnatdoc_enhancer.inventory import Package
        config = load(ROOT, 'docs/gnatdoc-enhance.toml')
        pkg = Package('P', Path('p.ads'), frozenset(), frozenset(), {}, [], 'package P is end P;')
        for raw in ('procedure F (S : String := "return T");', 'procedure F; -- return T\n'):
            result, count = linked(raw, pkg, {'p': pkg}, None, {'p.t': (Path('observed.html'), 'observed')}, config)
            self.assertEqual((result, count), (raw, 0))

    def test_removed_link_and_consistently_forged_manifest(self):
        # Removal leaves native bytes intact, so the semantic acceptance layer
        # alone cannot prove completeness. The separate exact reconstruction can.
        with tempfile.TemporaryDirectory() as temp:
            site = Path(temp) / 'site'
            presentation.build(ROOT / '_site', site)
            page = site / 'api' / self.page
            page.write_text(page.read_text().replace(self.markup, 'Complex_I16', 1))
            manifest = site / 'api/presentation.json'
            data = json.loads(manifest.read_text())
            data['output_sha256']['api/' + self.page] = presentation.sha(page.read_bytes())
            for counter in ('linked_types', 'linked_local_types'):
                data['counts'][counter] -= 1
            manifest.write_text(json.dumps(data))
            with self.assertRaises(ValueError):
                presentation.validate(site, ROOT / '_site')


if __name__ == '__main__':
    unittest.main()
