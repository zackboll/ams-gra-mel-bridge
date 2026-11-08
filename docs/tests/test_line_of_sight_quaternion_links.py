"""Focused current-source Image LineOfSightQuaternion visibility and type-link regression.

IRMEL-ADA-010 keeps the owned value a public record whose components reuse the canonical
Image Quaternion and ODM Forward_Right_Down types through subtypes, keeps the metadata owner a
limited private type distinct from the other Image metadata owners, and links every public
operation only to public declarations. Run with the pinned documentation enhancer interpreter;
no historical site or documentation campaign is needed.
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

PACKAGE = 'ams.mel.ir.line_of_sight_quaternion'
METADATA = 'ams.mel.ir.image.metadata'
VALUE = PACKAGE + '.line_of_sight_value'
QUATERNION = PACKAGE + '.quaternion'
FRD = PACKAGE + '.forward_right_down'
CANONICAL_QUATERNION = 'ams.mel.ir.image.quaternion'
CANONICAL_FRD = 'ams.mel.ir.optical_distortion_map.forward_right_down'
STREAM = METADATA + '.line_of_sight_quaternion_stream'
COUNTERS = METADATA + '.metadata_counters'
IMAGE = 'ams.mel.ir.image_stream'


class LineOfSightQuaternionLinkTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.config = load(ROOT, 'docs/gnatdoc-enhance.toml')
        cls.source = packages(cls.config)
        cls.package = cls.source[PACKAGE]
        cls.metadata = cls.source[METADATA]
        identities = (VALUE, QUATERNION, FRD, CANONICAL_QUATERNION, CANONICAL_FRD, STREAM,
                      COUNTERS, IMAGE, METADATA + '.nuc_stream',
                      METADATA + '.camera_response_stream',
                      METADATA + '.optical_distortion_map_stream', METADATA + '.metadata_stream')
        cls.coordinates = {name: (f'{index:064x}.html', f'{index + 100:064x}')
                           for index, name in enumerate(identities, 1)}
        cls.targets = {k: (Path(p), a) for k, (p, a) in cls.coordinates.items()}

    def render(self, package, declaration):
        return linked(html.escape(declaration), package, self.source, None,
                      self.targets, self.config)

    def links(self, package, signature):
        marked, _ = self.render(package, signature)
        return [m[2] for m in LINK.finditer(marked) if m[2] in self.coordinates]

    def test_value_record_and_private_owner(self):
        tokens = [t[0] for t in lex(self.package.public_types['line_of_sight_value'].signature)]
        self.assertEqual(tokens[:4], ['type', 'line_of_sight_value', 'is', 'record'])
        for field in ('system_time_ns', 'q_xyzw', 'pqr_rps'):
            self.assertIn(field, tokens)
        self.assertNotIn('interfaces.c', ' '.join(tokens))
        stream = self.metadata.public_types['line_of_sight_quaternion_stream'].signature
        self.assertEqual([t[0] for t in lex(stream)],
                         ['type', 'line_of_sight_quaternion_stream', 'is', 'limited', 'private',
                          ';'])

    def test_component_subtypes_link_to_canonical_records(self):
        quaternion = self.package.public_types['quaternion'].signature
        frd = self.package.public_types['forward_right_down'].signature
        # Each subtype links to exactly the one canonical record it reuses.
        self.assertEqual(self.links(self.package, quaternion), [CANONICAL_QUATERNION])
        self.assertEqual(self.links(self.package, frd), [CANONICAL_FRD])
        # The value record's components link to the local subtypes, never to C imports.
        value = self.package.public_types['line_of_sight_value'].signature
        self.assertEqual(self.links(self.package, value), [QUATERNION, FRD])

    def test_metadata_operations_link_only_to_the_quaternion_owner(self):
        expected = {
            'Open_Line_Of_Sight_Quaternion': [IMAGE, STREAM],
            'Receive_Line_Of_Sight_Quaternion': [STREAM, VALUE],
            'Line_Of_Sight_Quaternion_Counters': [STREAM, COUNTERS],
        }
        others = {METADATA + '.nuc_stream', METADATA + '.camera_response_stream',
                  METADATA + '.optical_distortion_map_stream', METADATA + '.metadata_stream'}
        checked = set()
        closes = 0
        for declaration in self.metadata.declarations:
            found = self.links(self.metadata, declaration.signature)
            if declaration.name in expected:
                with self.subTest(operation=declaration.name):
                    self.assertEqual(found, expected[declaration.name])
                    self.assertFalse(set(found) & others)
                    checked.add(declaration.name)
            elif declaration.name == 'Close' and STREAM in found:
                self.assertEqual(found, [STREAM])
                closes += 1
        self.assertEqual(checked, set(expected))
        self.assertEqual(closes, 1)


if __name__ == '__main__':
    unittest.main()
