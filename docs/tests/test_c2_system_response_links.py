"""Focused current-source C2 SystemTrackDataResponse visibility and type-link regression.

IRMEL-ADA-008 keeps the C2 observer and owned result as private types in their
own child package; they are never aliases of the private Track response owner.
Shared public value types are ordinary subtypes of public Track types, never of a
private implementation type. Run with the pinned documentation enhancer
interpreter; no historical site or documentation campaign is needed.
"""

import html
from pathlib import Path
import sys
import unittest

ROOT = Path(__file__).resolve().parents[2]
sys.path[:0] = [str(ROOT / 'docs/tools'), str(ROOT / 'scripts')]

from ada_gnatdoc_enhancer.config import load
from ada_gnatdoc_enhancer.inventory import packages
from ada_gnatdoc_enhancer.syntax import linked
from local_type_audit import LINK, lex

PACKAGE = 'ams.mel.ir.c2.system_data'
TRACK = 'ams.mel.ir.track.system_data'
REQUEST = PACKAGE + '.response_request'
RESULT = PACKAGE + '.response_result'
STATUS = PACKAGE + '.command_status'


class C2SystemResponseLinkTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.config = load(ROOT, 'docs/gnatdoc-enhance.toml')
        cls.source = packages(cls.config)
        cls.package = cls.source[PACKAGE]
        cls.coordinates = {
            REQUEST: ('a' * 64 + '.html', 'b' * 64),
            RESULT: ('c' * 64 + '.html', 'd' * 64),
            STATUS: ('e' * 64 + '.html', 'f' * 64),
            PACKAGE + '.system_track_data_response': ('1' * 64 + '.html', '2' * 64),
            TRACK + '.system_track_data_response': ('3' * 64 + '.html', '4' * 64),
            TRACK + '.response_request': ('5' * 64 + '.html', '6' * 64),
            'ams.mel.ir.c2.control_channel': ('7' * 64 + '.html', '8' * 64),
        }
        cls.targets = {k: (Path(p), a) for k, (p, a) in cls.coordinates.items()}

    def render(self, declaration):
        return linked(html.escape(declaration), self.package, self.source, None,
                      self.targets, self.config)

    def test_owners_are_distinct_private_types(self):
        for name in ('response_request', 'response_result', 'command_status'):
            with self.subTest(name=name):
                tokens = [t[0] for t in lex(self.package.public_types[name].signature)]
                self.assertEqual(tokens[-2:], ['private', ';'])
                self.assertNotIn('track', tokens)
        tokens = [t[0] for t in lex(self.package.public_types['response_request'].signature)]
        self.assertEqual(tokens, ['type', 'response_request', 'is', 'limited', 'private', ';'])

    def test_shared_input_is_subtype_of_public_track_value(self):
        declaration = self.package.public_types['system_track_data_response'].signature
        self.assertEqual([t[0] for t in lex(declaration)],
                         ['subtype', 'system_track_data_response', 'is', 'ams', '.', 'mel', '.',
                          'ir', '.', 'track', '.', 'system_data', '.',
                          'system_track_data_response', ';'])
        self.assertIn('system_track_data_response', self.source[TRACK].public_types)

    def test_operations_link_only_to_c2_owners(self):
        expected = {
            'Submit_System_Track_Data_Response': [
                'ams.mel.ir.c2.control_channel', PACKAGE + '.system_track_data_response', REQUEST],
            'Wait': [REQUEST, RESULT],
            'Is_Open': [REQUEST],
            'Close': [REQUEST],
        }
        checked = set()
        for declaration in self.package.declarations:
            if declaration.name not in expected:
                continue
            marked, _ = self.render(declaration.signature)
            found = [m[2] for m in LINK.finditer(marked) if m[2] in self.coordinates]
            with self.subTest(operation=declaration.name):
                self.assertEqual(found, expected[declaration.name])
                self.assertNotIn(TRACK + '.response_request', found)
                checked.add(declaration.name)
        self.assertEqual(checked, set(expected))


if __name__ == '__main__':
    unittest.main()
