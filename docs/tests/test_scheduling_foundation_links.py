"""Focused current-source Scheduling foundation visibility and type-link regression.

IRMEL-ADA-011 keeps the Scheduling configuration private, keeps the owner a limited private
type distinct from every other typed channel owner, links each public operation only to public
declarations (the Session, the owner, the shared ChannelCapability and common View), and exposes
no Scheduling task, track-list or metadata declaration. Run with the pinned documentation
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
from local_type_audit import LINK, lex

PACKAGE = 'ams.mel.ir.scheduling'
CONFIG = PACKAGE + '.scheduling_config'
OWNER = PACKAGE + '.scheduling_channel'
SESSION = 'ams.mel.session'
UCI = 'ams.mel.ir.uci_id'
LOCATION = 'ams.mel.ir.component_location'
CAPABILITY = 'ams.mel.ir.channel.channel_capability'
VIEW = 'ams.mel.ir.channel.view'
OTHER_OWNERS = ('ams.mel.ir.track.track_channel', 'ams.mel.ir.c2.control_channel',
                'ams.mel.ir.health_status.health_channel',
                'ams.mel.ir.instrumentation.instrumentation_channel')


class SchedulingFoundationLinkTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.config = load(ROOT, 'docs/gnatdoc-enhance.toml')
        cls.source = packages(cls.config)
        cls.package = cls.source[PACKAGE]
        identities = (CONFIG, OWNER, SESSION, UCI, LOCATION, CAPABILITY, VIEW) + OTHER_OWNERS
        cls.coordinates = {name: (f'{index:064x}.html', f'{index + 100:064x}')
                           for index, name in enumerate(identities, 1)}
        cls.targets = {k: (Path(p), a) for k, (p, a) in cls.coordinates.items()}

    def links(self, signature):
        marked, _ = linked(html.escape(signature), self.package, self.source, None,
                           self.targets, self.config)
        return [m[2] for m in LINK.finditer(marked) if m[2] in self.coordinates]

    def test_private_config_and_limited_owner(self):
        self.assertEqual([t[0] for t in lex(self.package.public_types['scheduling_config'].signature)],
                         ['type', 'scheduling_config', 'is', 'private', ';'])
        self.assertEqual([t[0] for t in lex(self.package.public_types['scheduling_channel'].signature)],
                         ['type', 'scheduling_channel', 'is', 'limited', 'private', ';'])
        self.assertEqual(set(self.package.public_types), {'scheduling_config', 'scheduling_channel'})

    def test_operations_link_only_to_public_declarations(self):
        expected = {
            'Create_Config': [UCI, UCI, LOCATION, CONFIG],
            'Open': [SESSION, CONFIG, OWNER],
            'Is_Open': [OWNER],
            'Enable': [OWNER],
            'Capabilities': [OWNER, CAPABILITY],
            'As_Channel': [OWNER, VIEW],
            'Close': [OWNER],
        }
        found = {}
        for declaration in self.package.declarations:
            if declaration.name in expected:
                found[declaration.name] = self.links(declaration.signature)
        self.assertEqual(found, expected)
        for name, links in found.items():
            with self.subTest(operation=name):
                self.assertFalse(set(links) & set(OTHER_OWNERS))

    def test_no_task_or_scheduling_metadata_surface(self):
        names = {d.name.lower() for d in self.package.declarations}
        self.assertEqual(names, {'create_config', 'open', 'is_open', 'enable', 'capabilities',
                                 'as_channel', 'close', 'scheduling_config',
                                 'scheduling_channel'})
        for forbidden in ('exec', 'task', 'disable', 'track_list', 'register', 'metadata'):
            self.assertFalse([n for n in names if forbidden in n], forbidden)


if __name__ == '__main__':
    unittest.main()
