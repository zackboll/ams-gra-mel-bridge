"""IRMEL-ADA-012 public owned acknowledgement and limited stream type-link regression."""
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

PACKAGE = 'ams.mel.ir.scheduling.metadata'
OWNER = 'ams.mel.ir.scheduling.scheduling_channel'
VALUE = PACKAGE + '.execute_task_ack'
STREAM = PACKAGE + '.execute_task_ack_stream'
COUNTERS = PACKAGE + '.metadata_counters'


class SchedulingAckLinks(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.config = load(ROOT, 'docs/gnatdoc-enhance.toml')
        cls.source = packages(cls.config)
        cls.package = cls.source[PACKAGE]
        cls.coordinates = {name: (f'{i:064x}.html', f'{i + 100:064x}')
                           for i, name in enumerate((OWNER, VALUE, STREAM, COUNTERS), 1)}
        cls.targets = {k: (Path(p), a) for k, (p, a) in cls.coordinates.items()}

    def test_value_and_limited_owner(self):
        types = self.package.public_types
        self.assertEqual([t[0] for t in lex(types['execute_task_ack'].signature)][:4],
                         ['type', 'execute_task_ack', 'is', 'record'])
        self.assertEqual([t[0] for t in lex(types['execute_task_ack_stream'].signature)],
                         ['type', 'execute_task_ack_stream', 'is', 'limited', 'private', ';'])
        signature = types['execute_task_ack'].signature.lower()
        self.assertIn('command_id', signature)
        self.assertIn('request_id', signature)
        self.assertNotIn('interfaces.c', signature)

    def test_public_operation_links(self):
        expected = {'Open_Execute_Task_Ack': [OWNER, STREAM],
                    'Receive_Execute_Task_Ack': [STREAM, VALUE],
                    'Execute_Task_Ack_Counters': [STREAM, COUNTERS], 'Close': [STREAM]}
        actual = {}
        closes = []
        for declaration in self.package.declarations:
            if declaration.name in expected:
                marked, _ = linked(html.escape(declaration.signature), self.package,
                                   self.source, None, self.targets, self.config)
                links = [m[2] for m in LINK.finditer(marked) if m[2] in self.coordinates]
                # IRMEL-ADA-013 adds an independent Close overload for the TaskExecutingRep
                # stream in the same package; collect every overload instead of the last one.
                if declaration.name == 'Close':
                    closes.append(links)
                else:
                    actual[declaration.name] = links
        actual['Close'] = [STREAM] if [STREAM] in closes else closes
        self.assertEqual(actual, expected)
        names = {d.name.lower() for d in self.package.declarations}
        self.assertFalse(names & {'exec', 'send', 'abort_task', 'match_request'})


if __name__ == '__main__':
    unittest.main()
