"""Focused current-source Image OpticalDistortionMap visibility and type-link regression.

IRMEL-ADA-009 keeps the owned map private with public accessors, keeps the
metadata owner a limited private type distinct from the other Image metadata
owners, and links every public operation only to public declarations. Run with
the pinned documentation enhancer interpreter; no historical site or
documentation campaign is needed.
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

PACKAGE = 'ams.mel.ir.optical_distortion_map'
METADATA = 'ams.mel.ir.image.metadata'
VALUE = PACKAGE + '.map_value'
FRD = PACKAGE + '.forward_right_down'
ROWCOL = PACKAGE + '.row_column'
STREAM = METADATA + '.optical_distortion_map_stream'
COUNTERS = METADATA + '.metadata_counters'
IMAGE = 'ams.mel.ir.image_stream'


class OpticalDistortionMapLinkTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.config = load(ROOT, 'docs/gnatdoc-enhance.toml')
        cls.source = packages(cls.config)
        cls.package = cls.source[PACKAGE]
        cls.metadata = cls.source[METADATA]
        identities = (VALUE, FRD, ROWCOL, STREAM, COUNTERS, IMAGE,
                      METADATA + '.nuc_stream', METADATA + '.camera_response_stream',
                      METADATA + '.metadata_stream')
        cls.coordinates = {name: (f'{index:064x}.html', f'{index + 100:064x}')
                           for index, name in enumerate(identities, 1)}
        cls.targets = {k: (Path(p), a) for k, (p, a) in cls.coordinates.items()}

    def render(self, package, declaration):
        return linked(html.escape(declaration), package, self.source, None,
                      self.targets, self.config)

    def links(self, package, signature):
        marked, _ = self.render(package, signature)
        return [m[2] for m in LINK.finditer(marked) if m[2] in self.coordinates]

    def test_owned_value_and_owner_are_private(self):
        self.assertEqual([t[0] for t in lex(self.package.public_types['map_value'].signature)],
                         ['type', 'map_value', 'is', 'private', ';'])
        stream = self.metadata.public_types['optical_distortion_map_stream'].signature
        self.assertEqual([t[0] for t in lex(stream)],
                         ['type', 'optical_distortion_map_stream', 'is', 'limited', 'private', ';'])
        # Public component records carry only named Long_Float components.
        for name, fields in (('forward_right_down', ['forward', 'right', 'down']),
                             ('row_column', ['row', 'column'])):
            tokens = [t[0] for t in lex(self.package.public_types[name].signature)]
            self.assertEqual(tokens[:4], ['type', name, 'is', 'record'])
            for field in fields:
                self.assertIn(field, tokens)
            self.assertNotIn('interfaces.c', ' '.join(tokens))

    def test_accessors_link_to_owned_types(self):
        expected = {
            'FRD_At': [VALUE, FRD],
            'RowCol_At': [VALUE, ROWCOL],
            'FRD_Count': [VALUE],
            'RowCol_Count': [VALUE],
            'Right_Min': [VALUE],
            'Pixels_Per_ODM': [VALUE],
            'Append_FRD': [VALUE, FRD],
            'Append_RowCol': [VALUE, ROWCOL],
        }
        checked = set()
        for declaration in self.package.declarations:
            if declaration.name in expected:
                with self.subTest(operation=declaration.name):
                    self.assertEqual(self.links(self.package, declaration.signature),
                                     expected[declaration.name])
                    checked.add(declaration.name)
        self.assertEqual(checked, set(expected))

    def test_metadata_operations_link_only_to_the_odm_owner(self):
        expected = {
            'Open_Optical_Distortion_Map': [IMAGE, STREAM],
            'Receive_Optical_Distortion_Map': [STREAM, VALUE],
            'Optical_Distortion_Map_Counters': [STREAM, COUNTERS],
        }
        others = {METADATA + '.nuc_stream', METADATA + '.camera_response_stream',
                  METADATA + '.metadata_stream'}
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
