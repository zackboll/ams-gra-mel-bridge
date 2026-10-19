"""Independent eligibility, completeness and binding on authentic source input."""

import copy
import json
from pathlib import Path
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'docs/tools'))
import build_enhanced_site as p
from local_type_audit import LINK, SECTION, lex, type_context, occurrence_key, require_complete_additions
from ada_gnatdoc_enhancer.config import load
from ada_gnatdoc_enhancer.pipeline import expected_output


class CompletenessTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.authority = p.Authority(ROOT, ROOT / '_site')
        # Establish expected occurrences BEFORE requesting any candidate HTML.
        cls.required = cls.authority.required_additions()
        _, cls.output, _ = expected_output(load(ROOT, 'docs/gnatdoc-enhance.toml'), ROOT / '_site')
        cls.audit = []
        for name, data in cls.output.items():
            if name.endswith('.html'):
                _, records = p.restore_page(Path(name).name, data.decode(), cls.authority)
                cls.audit.extend(records)

    def test_all_ten_authentic_occurrences(self):
        self.assertEqual(len(self.required), 10)
        require_complete_additions(self.audit, self.required)
        self.assertEqual(sum(r['context'] == 'subtype-base' for r in self.required), 1)
        self.assertEqual(sum(r['visible'].startswith('Image.') for r in self.required), 7)
        self.assertEqual(sum(r['visible'].startswith('Instrumentation.') for r in self.required), 2)
        self.assertEqual([(r['containing']['name'], r['source_line'], r['visible'])
                          for r in self.required if r['context'] != 'subtype-base'], [
            ('Sensor_Inertial_State', 98, 'Image.Uncertainty'),
            ('Sensor_Nav_State', 122, 'Image.Orientation'),
            ('Sensor_Nav_State', 124, 'Image.Orientation'),
            ('Sensor_Nav_State', 126, 'Image.Orientation'),
            ('Sensor_Nav_State', 128, 'Image.Coordinate_System'),
            ('Navigation_Report', 666, 'Image.Attitude_Rate'),
            ('Navigation_Report', 672, 'Image.Position_Velocity_Covariance'),
            ('Instrumentation_Level_Command', 87, 'Instrumentation.Priority'),
            ('Instrumentation_Report', 98, 'Instrumentation.Priority')])
        for r in self.required:
            self.assertIn(r, self.audit)
            self.assertEqual(r['href'], r['target']['page'] + '#' + r['target']['anchor'])
            self.assertIn('<h4 id=' + r['target']['anchor'] + '>' + r['target']['name'] + '</h4>',
                          (ROOT / '_site/api' / r['target']['page']).read_text())

    def test_navigation_response_real_record_target(self):
        r = next(r for r in self.required if r['context'] == 'subtype-base')
        self.assertEqual(r['containing']['package'], 'ams.mel.ir.image.metadata')
        self.assertEqual(r['target_identity'], 'ams.mel.ir.image.navigation_response')
        self.assertTrue(r['target']['native'].startswith('type Navigation_Response is record'))

    def test_navigation_report_six_links_and_real_record_destinations(self):
        records = [r for r in self.audit if r['containing']['name'] == 'Navigation_Report']
        self.assertEqual([r['visible'] for r in records],
                         ['Position_Solution_State', 'AMS.MEL.Status.Euler', 'Image.Attitude_Rate',
                          'North_East_Down', 'North_East_Down', 'Image.Position_Velocity_Covariance'])
        for r in records:
            if r['visible'].startswith('Image.'):
                self.assertIn('is record', r['target']['native'])
        native = records[0]['containing']['native']
        self.assertIn('Long_Float', native)
        self.assertIn('Long_Long_Integer', native)
        self.assertFalse(any(r['visible'] in ('Long_Float', 'Long_Long_Integer') for r in self.audit))

    def test_original_1418_complete_ordered_records_unchanged(self):
        keys = {occurrence_key(r) for r in self.required}
        old = [r for r in self.audit if occurrence_key(r) not in keys]
        self.assertEqual(len(old), 1418)
        self.assertEqual(p.fingerprint(old), p.ORIGINAL_AUDIT_SHA256)

    def test_every_removed_required_link_rejected_by_completeness_guard(self):
        for r in self.required:
            with self.subTest(visible=r['visible'], offset=r['native_token_start']):
                with self.assertRaisesRegex(ValueError, 'required type-link occurrence'):
                    require_complete_additions([q for q in self.audit if q != r], self.required)

    def test_every_wrong_existing_target_rejected_by_semantic_guard(self):
        wrong = next(r['target'] for r in self.audit if r['target_identity'] == 'ams.mel.status.euler')
        for r in self.required:
            with self.subTest(visible=r['visible']):
                with self.assertRaisesRegex(ValueError, 'wrong existing/native destination'):
                    self.authority.occurrence(r['containing']['page'], r['containing']['anchor'],
                                              r['containing']['native'], r['native_token_start'],
                                              r['native_token_end'], r['visible'],
                                              wrong['page'] + '#' + wrong['anchor'], r['target_identity'])

    def test_every_forged_identity_pair_rejected_by_semantic_guard(self):
        wrong = next(r for r in self.audit if r['target_identity'] == 'ams.mel.status.euler')
        for r in self.required:
            with self.subTest(visible=r['visible']), self.assertRaisesRegex(ValueError, 'forged type identity'):
                self.authority.occurrence(r['containing']['page'], r['containing']['anchor'],
                                          r['containing']['native'], r['native_token_start'],
                                          r['native_token_end'], r['visible'], wrong['href'],
                                          wrong['target_identity'])

    def test_incorrect_subtype_defining_context_rejected(self):
        r = next(r for r in self.required if r['context'] == 'subtype-base')
        a = copy.deepcopy(self.authority)
        entry = a.sections[r['containing']['page'], r['containing']['anchor']]
        entry['declaration'] = entry['declaration'].replace('subtype ', 'package ', 1)
        with self.assertRaisesRegex(ValueError, 'unsupported/non-type context'):
            a.occurrence(entry['page'], entry['anchor'], entry['native'], r['native_token_start'],
                         r['native_token_end'], r['visible'], r['href'], r['target_identity'])

    def test_private_subtype_target_rejected(self):
        r = next(r for r in self.required if r['context'] == 'subtype-base')
        a = copy.deepcopy(self.authority)
        a.types[r['target_identity']]['declaration'] = 'type Navigation_Response is private;'
        with self.assertRaisesRegex(ValueError, 'private ordinary subtype target'):
            a.occurrence(r['containing']['page'], r['containing']['anchor'], r['containing']['native'],
                         r['native_token_start'], r['native_token_end'], r['visible'], r['href'], r['target_identity'])

    def test_source_native_mismatch_rejected(self):
        for r in self.required:
            a = copy.deepcopy(self.authority)
            entry = a.sections[r['containing']['page'], r['containing']['anchor']]
            entry['declaration'] = 'X : Boolean := "' + r['visible'] + '";'
            with self.assertRaisesRegex(ValueError, 'source ownership|source projection|context disagrees'):
                a.occurrence(entry['page'], entry['anchor'], entry['native'], r['native_token_start'],
                             r['native_token_end'], r['visible'], r['href'], r['target_identity'])

    def test_alias_generic_ancestor_and_sibling_short_names_rejected(self):
        r = next(r for r in self.required if r['visible'] == 'Image.Attitude_Rate')
        for name in ('Alias.Attitude_Rate', 'IR.Attitude_Rate', 'Instrumentation.Priority',
                     'Instance.Attitude_Rate'):
            with self.subTest(name=name), self.assertRaisesRegex(ValueError, 'qualified owner not source-visible'):
                self.authority.destination(name, r['containing']['package'], r['containing'])

    def test_each_required_removal_with_self_consistent_manifest(self):
        with tempfile.TemporaryDirectory() as tmp:
            site = Path(tmp) / 'site'
            p.build(ROOT / '_site', site)
            for r in self.required:
                path = site / 'api' / r['containing']['page']
                original = path.read_bytes()
                text = original.decode()
                section = next(m for m in SECTION.finditer(text)
                               if m[2] == r['containing']['anchor'])
                prior = [q for q in self.required if q['containing']['anchor'] == r['containing']['anchor']
                         and q['visible'] == r['visible'] and q['native_token_start'] < r['native_token_start']]
                markup = [m for m in LINK.finditer(section[3]) if m[3] == r['visible']][len(prior)]
                body = section[3][:markup.start()] + r['visible'] + section[3][markup.end():]
                path.write_text(text[:section.start(3)] + body + text[section.end(3):])
                manifest = site / 'api/presentation.json'
                original_manifest = manifest.read_bytes()
                data = json.loads(original_manifest)
                data['output_sha256']['api/' + path.name] = p.sha(path.read_bytes())
                data['counts']['linked_types'] -= 1
                data['counts']['linked_local_types'] -= 1
                manifest.write_text(json.dumps(data))
                with self.subTest(visible=r['visible']), self.assertRaisesRegex(ValueError, 'required type-link occurrence'):
                    p.validate(site, ROOT / '_site')
                path.write_bytes(original)
                manifest.write_bytes(original_manifest)


def shadow_test(insertion):
    def test(self):
        r = next(r for r in self.required if r['visible'] == 'Image.Attitude_Rate')
        a = copy.deepcopy(self.authority)
        pkg = a.packages[r['containing']['package']]
        pkg['source'] = pkg['source'].replace('package AMS.MEL.IR.Image is',
                                              'package AMS.MEL.IR.Image is ' + insertion, 1)
        with self.assertRaisesRegex(ValueError, 'shadowed|unsupported'):
            a.destination(r['visible'], r['containing']['package'], r['containing'])
    return test


for name, source in {
        'object': 'Image : Integer;',
        'same_line_object': 'Other : Integer; Image : Integer;',
        'comma_list_first': 'Image, Other : Integer;',
        'comma_list_last': 'Other, Image : Integer;',
        'enum_literal': 'type Hidden is (Image, Other);',
        'formal': 'procedure F (Image : Integer);',
        'alias': 'package Image renames AMS.MEL.Status;',
        'generic_instance': 'package X is new Container (Integer);',
        'inherited_names': 'type X is new Some_Type;',
        'nested_package': 'package Nested is end Nested;',
        'same_line_private': 'private Image : Integer;',
        }.items():
    setattr(CompletenessTests, 'test_shadow_' + name, shadow_test(source))


def subtype_test(text):
    def test(self):
        start = text.index('AMS.MEL.IR.Image.Navigation_Response')
        tokens = lex(text)
        indices = [i for i, token in enumerate(tokens) if token[1] == start]
        with self.assertRaises((ValueError, IndexError)):
            type_context(tokens, indices[0], start + len('AMS.MEL.IR.Image.Navigation_Response'))
    return test


for name, text in {
        'object_initializer': 'X : Integer := AMS.MEL.IR.Image.Navigation_Response;',
        'package_renaming': 'package X renames AMS.MEL.IR.Image.Navigation_Response;',
        'expression': 'X := F (AMS.MEL.IR.Image.Navigation_Response);',
        'aspect': 'type X is private with Size => AMS.MEL.IR.Image.Navigation_Response;',
        'string': 'X : String := "AMS.MEL.IR.Image.Navigation_Response";',
        'comment': '-- subtype X is AMS.MEL.IR.Image.Navigation_Response;',
        'constraint': 'subtype X is AMS.MEL.IR.Image.Navigation_Response (1 .. 2);',
        'range': 'subtype X is AMS.MEL.IR.Image.Navigation_Response range 1 .. 2;',
        'generic': 'generic subtype X is AMS.MEL.IR.Image.Navigation_Response;',
        'unknown_is': 'package X is AMS.MEL.IR.Image.Navigation_Response;',
        }.items():
    setattr(CompletenessTests, 'test_subtype_reject_' + name, subtype_test(text))
