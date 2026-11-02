"""Focused current-source Camera request visibility and type-link regression.

Run with the pinned documentation enhancer interpreter; no historical site or
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
from local_type_audit import Authority, LINK, lex, restore_page
from gnatdoc_coverage_model import specification


class CameraRequestLinkTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.config = load(ROOT, 'docs/gnatdoc-enhance.toml')
        cls.source = packages(cls.config)
        cls.camera = cls.source['ams.mel.ir.camera']
        cls.identity = 'ams.mel.ir.camera.camera_request'
        cls.page = 'a' * 64 + '.html'
        cls.anchor = 'b' * 64
        cls.owner_page = 'c' * 64 + '.html'
        cls.owner_anchor = 'd' * 64
        cls.targets = {
            cls.identity: (Path(cls.page), cls.anchor),
            'ams.mel.ir.camera_request_owner': (Path(cls.owner_page), cls.owner_anchor),
        }

    def audit(self, package, declaration, marked):
        # Use real source declarations and the real renderer, with only native
        # page/anchor coordinates supplied by this small fixture.
        authority = Authority.__new__(Authority)
        entry = {'native': declaration, 'declaration': declaration,
                 'package': package.name.lower(), 'line': 1}
        authority.sections = {(self.page, self.anchor): entry}
        authority.packages = {
            name: {'declarations': [{'name': d.name} for d in pkg.declarations],
                   'imports': pkg.imports, 'uses': pkg.uses,
                   'source': pkg.source, 'name': pkg.name}
            for name, pkg in self.source.items()
        }
        authority.types = {
            self.identity: {'page': self.page, 'anchor': self.anchor,
                            'declaration': self.camera.public_types['camera_request'].signature},
            'ams.mel.ir.camera_request_owner': {
                'page': self.owner_page, 'anchor': self.owner_anchor,
                'declaration': self.source['ams.mel.ir'].public_types['camera_request_owner'].signature},
        }
        snippet = (f'<h4 id={self.anchor}>Request</h4>'
                   f'<pre class=ada-code-snippet><code>{marked}</code></pre>')
        return restore_page(self.page, snippet, authority)

    def render(self, package, declaration):
        return linked(html.escape(declaration), package, self.source, None,
                      self.targets, self.config)

    def test_public_request_derives_from_opaque_limited_owner(self):
        declaration = self.camera.public_types['camera_request']
        self.assertEqual(declaration.kind, 'type')
        _, lines, _, _, declarations, _ = specification(self.camera.path)
        request = next(d for d in declarations if d['name'] == 'Camera_Request')
        native = '\n'.join(lines[request['start'] - 1:request['end']]).strip()
        self.assertEqual([t[0] for t in lex(native)],
                         ['type', 'camera_request', 'is', 'new', 'ams', '.', 'mel', '.',
                          'ir', '.', 'camera_request_owner', 'with', 'private', ';'])
        parent = self.source['ams.mel.ir'].public_types['camera_request_owner']
        self.assertEqual([t[0] for t in lex(parent.signature)],
                         ['type', 'camera_request_owner', 'is', 'tagged', 'limited', 'private', ';'])
        marked, count = self.render(self.camera, native)
        self.assertEqual(count, 1)
        _, records = self.audit(self.camera, native, marked)
        self.assertEqual([(r['context'], r['target_identity']) for r in records],
                         [('derived', 'ams.mel.ir.camera_request_owner')])

    def test_both_submissions_and_request_operations_link_to_public_type(self):
        checked = []
        for package_name, names in (
            ('ams.mel.ir.camera', {'Is_Open', 'Wait', 'Close'}),
            ('ams.mel.ir.c2', {'Submit_Camera_Command'}),
            ('ams.mel.ir.image', {'Submit_Camera_Command'}),
        ):
            package = self.source[package_name]
            for declaration in package.declarations:
                if declaration.name not in names:
                    continue
                with self.subTest(package=package_name, operation=declaration.name):
                    marked, count = self.render(package, declaration.signature)
                    self.assertEqual(count, 1)
                    self.assertEqual([m[2] for m in LINK.finditer(marked)], [self.identity])
                    _, records = self.audit(package, declaration.signature, marked)
                    self.assertEqual([r['target_identity'] for r in records], [self.identity])
                    checked.append((package_name, declaration.name))
        self.assertEqual(len(checked), 5)

    def test_original_private_ordinary_subtype_link_still_rejected(self):
        original = 'subtype Camera_Request is AMS.MEL.IR.Camera_Request_Owner;'
        marked, count = self.render(self.camera, original)
        self.assertEqual(count, 1)
        with self.assertRaisesRegex(ValueError, 'private ordinary subtype target'):
            self.audit(self.camera, original, marked)


if __name__ == '__main__':
    unittest.main()
