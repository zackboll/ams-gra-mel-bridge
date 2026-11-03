"""Focused current-source CalibrationConfiguration request visibility and type-link regression.

IRMEL-ADA-005 deliberately avoids the IRMEL-ADA-003 R3 documentation defect: the
public request is a type derived from an opaque limited owner, never an ordinary
subtype of a private implementation owner. Run with the pinned documentation
enhancer interpreter; no historical site or documentation campaign is needed.
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

PACKAGE = 'ams.mel.ir.calibration_configuration'
REQUEST = PACKAGE + '.configuration_request'
OWNER = 'ams.mel.ir.calibration_configuration_request_owner'
STREAM = 'ams.mel.ir.c2.metadata.calibration_configuration_stream'


class CalibrationConfigurationRequestLinkTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.config = load(ROOT, 'docs/gnatdoc-enhance.toml')
        cls.source = packages(cls.config)
        cls.configuration = cls.source[PACKAGE]
        cls.page = 'a' * 64 + '.html'
        cls.anchor = 'b' * 64
        cls.coordinates = {
            REQUEST: ('a' * 64 + '.html', 'b' * 64),
            OWNER: ('c' * 64 + '.html', 'd' * 64),
            STREAM: ('e' * 64 + '.html', 'f' * 64),
            PACKAGE + '.configuration_command': ('1' * 64 + '.html', '2' * 64),
            PACKAGE + '.calibration_collection': ('3' * 64 + '.html', '4' * 64),
            PACKAGE + '.configuration_result': ('5' * 64 + '.html', '6' * 64),
            PACKAGE + '.subsystem_calibration': ('7' * 64 + '.html', '8' * 64),
            PACKAGE + '.accepted_interface': ('9' * 64 + '.html', '0' * 64),
        }
        cls.targets = {k: (Path(p), a) for k, (p, a) in cls.coordinates.items()}

    def declaration_of(self, identity):
        package, name = identity.rsplit('.', 1)
        return self.source[package].public_types[name].signature

    def audit(self, package, declaration, marked):
        # Real source declarations and the real renderer; only native page/anchor
        # coordinates are supplied by this fixture.
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
            identity: {'page': page, 'anchor': anchor,
                       'declaration': self.declaration_of(identity)}
            for identity, (page, anchor) in self.coordinates.items()
        }
        snippet = (f'<h4 id={self.anchor}>Declaration</h4>'
                   f'<pre class=ada-code-snippet><code>{marked}</code></pre>')
        return restore_page(self.page, snippet, authority)

    def render(self, package, declaration):
        return linked(html.escape(declaration), package, self.source, None,
                      self.targets, self.config)

    def test_public_request_derives_from_opaque_limited_owner(self):
        declaration = self.configuration.public_types['configuration_request']
        self.assertEqual(declaration.kind, 'type')
        _, lines, _, _, declarations, _ = specification(self.configuration.path)
        request = next(d for d in declarations if d['name'] == 'Configuration_Request')
        native = '\n'.join(lines[request['start'] - 1:request['end']]).strip()
        self.assertEqual([t[0] for t in lex(native)],
                         ['type', 'configuration_request', 'is', 'new', 'ams', '.', 'mel', '.',
                          'ir', '.', 'calibration_configuration_request_owner', 'with', 'private', ';'])
        parent = self.source['ams.mel.ir'].public_types['calibration_configuration_request_owner']
        self.assertEqual([t[0] for t in lex(parent.signature)],
                         ['type', 'calibration_configuration_request_owner', 'is', 'tagged', 'limited',
                          'private', ';'])
        marked, count = self.render(self.configuration, native)
        self.assertEqual(count, 1)
        _, records = self.audit(self.configuration, native, marked)
        self.assertEqual([(r['context'], r['target_identity']) for r in records],
                         [('derived', OWNER)])

    def test_metadata_owner_is_limited_private(self):
        stream = self.source['ams.mel.ir.c2.metadata'].public_types['calibration_configuration_stream']
        self.assertEqual([t[0] for t in lex(stream.signature)],
                         ['type', 'calibration_configuration_stream', 'is', 'limited', 'private', ';'])

    def test_operations_link_only_to_public_types(self):
        expected = {
            (PACKAGE, 'Is_Open'): [REQUEST],
            (PACKAGE, 'Wait'): [REQUEST, PACKAGE + '.configuration_result'],
            (PACKAGE, 'Close'): [REQUEST],
            ('ams.mel.ir.c2', 'Submit_Calibration_Configuration'):
                [PACKAGE + '.configuration_command', REQUEST],
            ('ams.mel.ir.c2.metadata', 'Open_Calibration_Configuration'): [STREAM],
            ('ams.mel.ir.c2.metadata', 'Receive_Calibration_Configuration'):
                [STREAM, PACKAGE + '.calibration_collection'],
        }
        checked = set()
        for (package_name, name), identities in expected.items():
            package = self.source[package_name]
            for declaration in package.declarations:
                if declaration.name != name:
                    continue
                marked, _ = self.render(package, declaration.signature)
                found = [m[2] for m in LINK.finditer(marked) if m[2] in self.coordinates]
                if not set(found) & {REQUEST, STREAM}:
                    continue  # an unrelated overload (for example another Close)
                with self.subTest(package=package_name, operation=name):
                    self.assertEqual(found, identities)
                    _, records = self.audit(package, declaration.signature, marked)
                    linked_identities = [r['target_identity'] for r in records
                                         if r['target_identity'] in self.coordinates]
                    self.assertEqual(linked_identities, identities)
                    checked.add((package_name, name))
        self.assertEqual(checked, set(expected))

    def test_nested_calibration_type_links(self):
        entry = self.configuration.public_types['subsystem_calibration'].signature
        marked, _ = self.render(self.configuration, entry)
        self.assertIn(PACKAGE + '.accepted_interface',
                      [m[2] for m in LINK.finditer(marked)])
        operation = next(d for d in self.configuration.declarations if d.name == 'Element')
        marked, _ = self.render(self.configuration, operation.signature)
        self.assertIn(PACKAGE + '.subsystem_calibration',
                      [m[2] for m in LINK.finditer(marked)])

    def test_private_ordinary_subtype_link_is_rejected(self):
        rejected = 'subtype Configuration_Request is AMS.MEL.IR.Calibration_Configuration_Request_Owner;'
        marked, count = self.render(self.configuration, rejected)
        # Public container instantiations make this package non-flat; the pinned
        # renderer conservatively declines an ordinary-subtype base altogether.
        self.assertEqual(count, 0)
        # Even if another renderer supplies that link, the source-backed audit
        # must reject it, exactly as for the earlier request-owner regressions.
        page, anchor = self.coordinates[OWNER]
        marked = rejected.replace('AMS.MEL.IR.Calibration_Configuration_Request_Owner',
            f'<a class="type-reference" href="{page}#{anchor}" data-type="{OWNER}">'
            'AMS.MEL.IR.Calibration_Configuration_Request_Owner</a>')
        with self.assertRaisesRegex(ValueError, 'unsupported nested/alias/instance public scope'):
            self.audit(self.configuration, rejected, marked)


if __name__ == '__main__':
    unittest.main()
