"""IRMEL-ADA-013 public TaskExecutingRep report, enumerations and limited stream type links."""
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
REPORT = PACKAGE + '.task_executing_report'
STREAM = PACKAGE + '.task_executing_stream'
KIND = PACKAGE + '.task_kind'
STATUS = PACKAGE + '.task_completion_status'
COUNTERS = PACKAGE + '.metadata_counters'
ACK_STREAM = PACKAGE + '.execute_task_ack_stream'
ACK = PACKAGE + '.execute_task_ack'


class SchedulingTaskExecutingLinks(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.config = load(ROOT, 'docs/gnatdoc-enhance.toml')
        cls.source = packages(cls.config)
        cls.package = cls.source[PACKAGE]
        identities = (OWNER, REPORT, STREAM, KIND, STATUS, COUNTERS, ACK_STREAM, ACK)
        cls.coordinates = {name: (f'{i:064x}.html', f'{i + 100:064x}')
                           for i, name in enumerate(identities, 1)}
        cls.targets = {k: (Path(p), a) for k, (p, a) in cls.coordinates.items()}

    def links(self, signature):
        marked, _ = linked(html.escape(signature), self.package, self.source, None,
                           self.targets, self.config)
        return [m[2] for m in LINK.finditer(marked) if m[2] in self.coordinates]

    def test_public_types_are_distinct_and_owned(self):
        types = self.package.public_types
        self.assertEqual([t[0] for t in lex(types['task_executing_report'].signature)][:4],
                         ['type', 'task_executing_report', 'is', 'record'])
        self.assertEqual([t[0] for t in lex(types['task_executing_stream'].signature)],
                         ['type', 'task_executing_stream', 'is', 'limited', 'private', ';'])
        report = types['task_executing_report'].signature.lower()
        for field in ('command_id', 'activity_id', 'request_id', 'current_task',
                      'estimated_duration_ns', 'track_id', 'previous_activity_id',
                      'previous_request_id', 'previous_task', 'previous_task_status'):
            self.assertIn(field, report)
        self.assertNotIn('interfaces.c', report)
        self.assertNotIn('duration :', report)
        kind = types['task_kind'].signature.lower()
        for literal in ('no_task', 'irst_update', 'reserved_1', 'reserved_6'):
            self.assertIn(literal, kind)
        self.assertNotIn('num_task_types', kind)
        self.assertIn('skipped', types['task_completion_status'].signature.lower())
        # The report is not the acknowledgement and the two streams are separate types.
        self.assertNotEqual(types['task_executing_report'].signature,
                            types['execute_task_ack'].signature)
        self.assertIn('execute_task_ack_stream', types)

    def test_report_fields_link_to_both_enumerations(self):
        self.assertEqual(self.links(self.package.public_types['task_executing_report'].signature),
                         [KIND, KIND, STATUS])

    def test_public_operation_links(self):
        expected = {'Open_Task_Executing': [OWNER, STREAM],
                    'Receive_Task_Executing': [STREAM, REPORT],
                    'Task_Executing_Counters': [STREAM, COUNTERS],
                    'Is_Operational': [KIND]}
        actual = {}
        closes = []
        for declaration in self.package.declarations:
            if declaration.name in expected:
                actual[declaration.name] = self.links(declaration.signature)
            if declaration.name == 'Close':
                closes.append(self.links(declaration.signature))
        self.assertEqual(actual, expected)
        self.assertEqual(sorted(closes), sorted([[ACK_STREAM], [STREAM]]))
        names = {d.name.lower() for d in self.package.declarations}
        self.assertFalse(names & {'exec', 'send', 'abort_task_request', 'match_request',
                                  'submit', 'schedule'})


if __name__ == '__main__':
    unittest.main()
