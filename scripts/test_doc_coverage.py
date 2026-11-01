"""Provider-free prose evidence and source/native boundary regressions."""

import html
import json
from pathlib import Path
import tempfile
import unittest

from check_doc_coverage import (
    CoverageError, ROOT, evidence_matches, measure, ratchet, source_prose,
)


class ProseTests(unittest.TestCase):
    def test_identifier_underscores_are_significant(self):
        for identifier in ('uint8_t', 'Field_Name', 'AMS.MEL.IR.Health_Status.Metadata'):
            with self.subTest(identifier=identifier):
                source = source_prose(['The ' + identifier + ' value.'])
                self.assertEqual(source, 'The ' + identifier + ' value.')
                self.assertTrue(evidence_matches(source, source))
                self.assertFalse(evidence_matches(source.replace('_', ''), source))
                self.assertFalse(evidence_matches(source, source.replace('_', '')))

    def test_backtick_identifiers_preserve_exact_contents(self):
        for marked, plain in (('`Field_Name`', 'Field_Name'),
                              ('``uint8_t``', 'uint8_t'),
                              ('`*Field_Name*`', '*Field_Name*')):
            with self.subTest(marked=marked):
                source = source_prose(['The ' + marked + ' value.'])
                self.assertEqual(source, 'The ' + plain + ' value.')
                self.assertTrue(evidence_matches('The ' + plain + ' value.', source))
        self.assertFalse(evidence_matches('The FieldName value.', source_prose(['The `Field_Name` value.'])))
        self.assertFalse(evidence_matches('The Field_Name value.', source_prose(['The `*Field_Name*` value.'])))

    def test_paired_emphasis_not_intraword_or_unpaired_markers(self):
        for marked in ('*owned value*', '**owned value**', '_owned value_', '__owned value__'):
            with self.subTest(marked=marked):
                source = source_prose(['The ' + marked + ' survives.'])
                self.assertEqual(source, 'The owned value survives.')
                self.assertTrue(evidence_matches('The owned value survives.', source))
        for text in ('Field_Name', 'uint8_t', 'a*b', '*unclosed', 'unclosed_'):
            self.assertEqual(source_prose(['The ' + text + ' value.']), 'The ' + text + ' value.')
        self.assertEqual(source_prose(['The **`Field_Name` value** survives.']),
                         'The Field_Name value survives.')
        self.assertEqual(source_prose(['The *`*Field_Name*` value* survives.']),
                         'The *Field_Name* value survives.')

    def test_whitespace_and_identifier_boundaries(self):
        self.assertTrue(evidence_matches('The\n uint8_t\t value.', source_prose([' The uint8_t ', 'value. '])))
        self.assertFalse(evidence_matches('Field_Name value', 'Name value'))
        self.assertFalse(evidence_matches('The Field_Name', 'The Field'))

    def test_missing_meaningless_and_wrong_declaration_prose(self):
        for block in ([], [''], ['...'], ['Value'], ['@param Value', '@return']):
            with self.subTest(block=block):
                self.assertFalse(evidence_matches('A fabricated rendered description.', source_prose(block)))
        self.assertFalse(evidence_matches('Closes a native owner.', source_prose(['Opens a native owner.'])))

    def test_tags_remain_attached_to_category_and_name(self):
        block = ['Owned source values.', '@param First The uint8_t input.',
                 '@param Second The uint16_t input.', '@return Owned result values.',
                 '@field Field_Name Exact uint32_t field.', '@enum Mode_Name Selects owned mode.']
        for tag, name, expected in (('param', 'First', 'The uint8_t input.'),
                                    ('return', None, 'Owned result values.'),
                                    ('field', 'Field_Name', 'Exact uint32_t field.'),
                                    ('enum', 'Mode_Name', 'Selects owned mode.')):
            with self.subTest(tag=tag):
                self.assertEqual(source_prose(block, tag, name), expected)
                self.assertFalse(evidence_matches('Unrelated rendered member prose.', source_prose(block, tag, name)))
                self.assertEqual(source_prose(block, tag, 'Wrong_Name'), '')
        self.assertFalse(evidence_matches('The uint16_t input.', source_prose(block, 'param', 'First')))


class CoverageBoundaryTests(unittest.TestCase):
    def setUp(self):
        temp = tempfile.TemporaryDirectory()
        self.addCleanup(temp.cleanup)
        self.root = Path(temp.name)
        (self.root / 'ada/src').mkdir(parents=True)
        (self.root / 'docs').mkdir()
        self.api = self.root / 'site/api'
        self.api.mkdir(parents=True)
        self.exceptions = json.loads((ROOT / 'docs/gnatdoc-renderer-exceptions.json').read_text())
        self.write_exceptions()
        (self.root / 'ada/src/ams-mel-ir-health_status-metadata.ads').write_text(
            'package AMS.MEL.IR.Health_Status.Metadata is\n'
            '   --  Owned health metadata.\n\n'
            '   package Status renames AMS.MEL.Status;\n'
            '   --  Public status alias.\n'
            'end AMS.MEL.IR.Health_Status.Metadata;\n')
        (self.api / 'health.html').write_text(
            '<h1>AMS.MEL.IR.Health_Status.Metadata</h1><h2>Description</h2><p>Owned health metadata.')
        self.spec = self.root / 'ada/src/demo.ads'
        self.spec.write_text('package Demo is\n   --  Owned demo values.\n\n'
                             '   function Read (Value : Natural) return Natural;\n'
                             '   --  Reads a Field_Name value.\n'
                             '   --  @param Value The uint8_t input.\n'
                             '   --  @return Owned result values.\nend Demo;\n')
        self.section = ('<h4 id=' + 'a' * 64 + '>Read</h4><pre class=ada-code-snippet><code>'
                        + html.escape('function Read (Value : Natural) return Natural;')
                        + '</code></pre><p>Reads a Field_Name value.'
                        '<h5>Parameters</h5><dl><dt>Value<dd>The uint8_t input.</dl>'
                        '<h5>Return Value</h5><dl><dd>Owned result values.</dl>')
        self.page = self.api / 'demo.html'
        self.header = '<h1>Demo</h1><h2>Description</h2><p>Owned demo values.'
        self.page.write_text(self.header + self.section)

    def write_exceptions(self):
        (self.root / 'docs/gnatdoc-renderer-exceptions.json').write_text(json.dumps(self.exceptions))

    def report(self):
        return measure(self.root / 'site', self.root)

    def test_exact_reconciliation_and_only_existing_exception(self):
        report = self.report()
        self.assertEqual(report['source_declarations'], report['native_sections'] + 1)
        self.assertEqual(report['undocumented_elements'], 1)
        self.assertEqual(report['renderer_exceptions'], self.exceptions['exceptions'])
        baseline = next(e['identity'] for e in report['elements'] if not e['documented']) + '\n'
        ratchet(report, baseline)
        for changed in ('', baseline + 'Demo: invented gap\n'):
            with self.assertRaises(CoverageError):
                ratchet(report, changed)

    def test_missing_and_duplicate_native_sections_fail(self):
        for sections in ('', self.section * 2):
            self.page.write_text(self.header + sections)
            with self.subTest(sections=len(sections)), self.assertRaises(CoverageError):
                self.report()

    def test_missing_source_cannot_borrow_rendered_prose(self):
        self.spec.write_text(self.spec.read_text().replace('   --  Reads a Field_Name value.\n', ''))
        element = next(e for e in self.report()['elements'] if 'Read:' in e['identity'] and e['kind'] == 'function')
        self.assertFalse(element['documented'])

    def test_wrong_parameter_description_is_a_gap(self):
        self.page.write_text((self.header + self.section).replace('The uint8_t input.', 'The uint8t input.'))
        element = next(e for e in self.report()['elements'] if e['kind'] == 'parameter')
        self.assertFalse(element['documented'])

    def test_wrong_field_and_enum_descriptions_remain_gaps(self):
        self.spec.write_text('package Demo is\n   --  Owned demo values.\n\n'
                             '   type Values is record\n      Field_Name : Natural;\n   end record;\n'
                             '   --  Owned field values.\n   --  @field Field_Name Exact uint8_t field.\n'
                             '   type Mode is (First_Mode, Second_Mode);\n'
                             '   --  Selects owned mode.\n   --  @enum First_Mode First owned mode.\n'
                             '   --  @enum Second_Mode Second owned mode.\nend Demo;\n')
        sections = ('<h4 id=' + 'a' * 64 + '>Values</h4><pre class=ada-code-snippet><code>'
                    'type Values is record Field_Name : Natural; end record;</code></pre>'
                    '<p>Owned field values.<h5>Record fields</h5><dl>'
                    '<dt>Field_Name<dd>Exact uint8_t field.</dl>'
                    '<h4 id=' + 'b' * 64 + '>Mode</h4><pre class=ada-code-snippet><code>'
                    'type Mode is (First_Mode, Second_Mode);</code></pre><p>Selects owned mode.'
                    '<h5>Enumeration Literal</h5><dl><dt>First_Mode<dd>First owned mode.'
                    '<dt>Second_Mode<dd>Second owned mode.</dl>')
        self.page.write_text(self.header + sections)
        self.assertEqual(self.report()['undocumented_elements'], 1)
        self.page.write_text((self.header + sections).replace('Exact uint8_t field.', 'Exact uint8t field.')
                            .replace('First owned mode.', 'Second owned mode.', 1))
        report = self.report()
        gaps = [e for e in report['elements'] if not e['documented']]
        self.assertEqual({e['kind'] for e in gaps}, {'renderer-exception', 'component', 'literal'})

    def test_wrong_declaration_summary_and_missing_member_fail(self):
        self.page.write_text((self.header + self.section).replace('Reads a Field_Name value.', 'Closes a native owner.'))
        element = next(e for e in self.report()['elements'] if e['kind'] == 'function')
        self.assertFalse(element['documented'])
        self.page.write_text((self.header + self.section).replace('<h5>Parameters</h5>', '<h5>Lost</h5>'))
        with self.assertRaises(CoverageError):
            self.report()

    def test_unknown_exception_and_changed_anchor_fail(self):
        report = self.report()
        self.page.write_text((self.header + self.section).replace('a' * 64, 'b' * 64))
        with self.assertRaises(CoverageError):
            measure(self.root / 'site', self.root, report['native'])
        self.exceptions['exceptions'].append(dict(self.exceptions['exceptions'][0]))
        self.write_exceptions()
        with self.assertRaises(CoverageError):
            self.report()


if __name__ == '__main__':
    unittest.main()