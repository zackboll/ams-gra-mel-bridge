"""Current-source static enumeration subtype proof, without renderer authority."""

import html
from pathlib import Path
import sys
import unittest

ROOT = Path(__file__).resolve().parents[2]
sys.path[:0] = [str(ROOT / 'docs/tools'), str(ROOT / 'scripts')]
from gnatdoc_coverage_model import specification
from local_type_audit import Authority, lex, restore_page, require_complete_additions

PAGE = 'a' * 64 + '.html'
ANCHOR = 'b' * 64
TARGET = 'c' * 64
PACKAGE = 'ams.mel.ir.scheduling.metadata'
IDENTITY = PACKAGE + '.task_kind'
HREF = PAGE + '#' + TARGET


class StaticSubtypeTests(unittest.TestCase):
    def setUp(self):
        path = ROOT / 'ada/src/ams-mel-ir-scheduling-metadata.ads'
        _, lines, _, _, declarations, _ = specification(path)
        self.declarations = declarations
        self.source = '\n'.join(lines)
        self.raw = {}
        for name in ('Task_Kind', 'Operational_Task_Kind'):
            decl = next(d for d in declarations if d['name'] == name)
            self.raw[name] = '\n'.join(lines[decl['start'] - 1:decl['end']]).strip()
        self.native = self.raw['Operational_Task_Kind'] + '\n'

    def authority(self, native=None, target=None):
        native = self.native if native is None else native
        target = self.raw['Task_Kind'] if target is None else target
        a = Authority.__new__(Authority)
        a.packages = {PACKAGE: {'name': PACKAGE, 'source': self.source,
                               'declarations': self.declarations, 'imports': set(), 'uses': set()}}
        a.sections = {(PAGE, ANCHOR): {
            'native': native, 'declaration': native, 'package': PACKAGE,
            'line': 144, 'name': 'Operational_Task_Kind', 'page': PAGE, 'anchor': ANCHOR}}
        a.types = {IDENTITY: {'native': target, 'declaration': target, 'package': PACKAGE,
                              'name': 'Task_Kind', 'page': PAGE, 'anchor': TARGET}}
        return a

    def snippet(self, native=None, token='Task_Kind', href=HREF, identity=IDENTITY):
        native = self.native if native is None else native
        start = native.index(token, native.index(' is ') + 4)
        wrapper = f'<a class="type-reference" href="{href}" data-type="{identity}">{token}</a>'
        body = html.escape(native[:start]) + wrapper + html.escape(native[start + len(token):])
        return f'<h4 id={ANCHOR}>Operational_Task_Kind</h4><pre class=ada-code-snippet><code>{body}</code></pre>'

    def restore(self, a=None, snippet=None):
        return restore_page(PAGE, self.snippet() if snippet is None else snippet,
                            self.authority() if a is None else a)

    def reject(self, a=None, snippet=None, message='independent type audit'):
        with self.assertRaisesRegex(ValueError, message):
            self.restore(a, snippet)

    def test_exact_source_range_public_destination_and_restoration(self):
        self.assertEqual(self.native.strip(),
                         'subtype Operational_Task_Kind is Task_Kind range No_Task .. IRST_Update;')
        a = self.authority()
        restored, records = self.restore(a)
        self.assertEqual(restored, self.snippet().replace(
            f'<a class="type-reference" href="{HREF}" data-type="{IDENTITY}">Task_Kind</a>', 'Task_Kind'))
        self.assertEqual(len(records), 1)
        record = records[0]
        self.assertEqual((record['href'], record['target_identity'], record['context']),
                         (HREF, IDENTITY, 'subtype-base'))
        self.assertEqual(record['containing']['declaration'], self.native)
        self.assertEqual(self.native[record['source_token_start']:record['source_token_end']], 'Task_Kind')
        self.assertEqual(a.required_additions(), records)
        require_complete_additions(records, a.required_additions())
        enum = [t[0] for t in lex(self.raw['Task_Kind'])][4:-2:2]
        self.assertEqual(enum.index('irst_update') - enum.index('no_task') + 1, 11)

    def test_unconstrained_ordinary_subtype(self):
        native = 'subtype Operational_Task_Kind is Task_Kind;\n'
        a = self.authority(native)
        a.packages[PACKAGE]['source'] = self.source.replace(self.native.strip(), native.strip())
        _, records = self.restore(a, self.snippet(native))
        self.assertEqual(len(records), 1)

    def test_native_abbreviated_enum_uses_complete_source_bounds(self):
        a = self.authority()
        a.types[IDENTITY]['native'] = ('type Task_Kind is\n'
            '  (No_Task, Abort_Task, IRST_Camera_Calibration, Camera_Calibration,\n'
            '   IRST_Boresight, Idle, IRST_Scan, IRST_Position, …, Reserved_6);\n')
        self.restore(a)
        a.types[IDENTITY]['native'] = a.types[IDENTITY]['native'].replace('Reserved_6', 'Forged')
        self.reject(a, message='source/native enumeration disagrees')

    def test_wrong_destination_identity_visible_text_and_complete_mark(self):
        snippet = self.snippet()
        for forged in (snippet.replace(HREF, PAGE + '#' + 'd' * 64),
                       snippet.replace(IDENTITY, PACKAGE + '.unknown'),
                       snippet.replace('>Task_Kind</a>', '>Unknown</a>'),
                       snippet.replace('>Task_Kind</a>', '>Task</a>_Kind')):
            with self.subTest(forged=forged):
                self.reject(snippet=forged)

    def test_unknown_ambiguous_and_invisible_base(self):
        a = self.authority()
        del a.types[IDENTITY]
        self.reject(a)
        a = self.authority()
        a.packages[PACKAGE]['declarations'] = self.declarations + [{'name': 'Task_Kind'}]
        self.reject(a, message='ambiguous')
        native = self.native.replace('is Task_Kind', 'is Invisible.Task_Kind')
        a = self.authority(native)
        a.types['invisible.task_kind'] = a.types[IDENTITY]
        self.reject(a, self.snippet(native, 'Invisible.Task_Kind'), 'not source-visible')

    def test_private_or_unproved_public_ownership(self):
        a = self.authority(target='type Task_Kind is private;')
        self.reject(a, message='private ordinary subtype target')
        for name in ('Task_Kind', 'Operational_Task_Kind'):
            a = self.authority()
            # Retain the declaration in the inventory but move it out of public source.
            a.packages[PACKAGE]['source'] = self.source.replace(self.raw[name], '')
            self.reject(a, message='unproved public')
        a = self.authority()
        a.packages[PACKAGE]['source'] = 'package ' + PACKAGE + ' is private\n' + self.source
        self.reject(a)

    def test_invalid_bounds_and_unsupported_constraints(self):
        for constraint in ('range Unknown .. IRST_Update', 'range No_Task .. Task_Kind',
                           'range 0 .. 10', "range Task_Kind'First .. IRST_Update",
                           'range No_Task .. IRST_Update + 1', 'range F (No_Task) .. IRST_Update',
                           'range No_Task ..', 'range .. IRST_Update', 'range No_Task . IRST_Update',
                           'range No_Task .. IRST_Update with Dynamic_Predicate => True',
                           '(No_Task .. IRST_Update)', 'range No_Task .. Other.IRST_Update'):
            native = 'subtype Operational_Task_Kind is Task_Kind ' + constraint + ';\n'
            a = self.authority(native)
            a.packages[PACKAGE]['source'] = self.source.replace(self.native.strip(), native.strip())
            with self.subTest(constraint=constraint):
                self.reject(a, self.snippet(native))

    def test_non_enum_base_and_source_native_disagreement(self):
        a = self.authority(target='type Task_Kind is range 0 .. 16;')
        a.packages[PACKAGE]['source'] = self.source.replace(self.raw['Task_Kind'],
                                                           a.types[IDENTITY]['declaration'])
        self.reject(a, message='unsupported range base enumeration')
        a = self.authority()
        a.sections[PAGE, ANCHOR]['declaration'] = self.native.replace('IRST_Update', 'Reserved_6')
        self.reject(a, message='source/native subtype disagrees')
        a = self.authority()
        a.packages[PACKAGE]['declarations'] = self.declarations + [{'name': 'No_Task', 'signature': 'No_Task : Integer;'}]
        self.reject(a, message='shadowed enumeration range bound')
        a = self.authority()
        a.packages[PACKAGE]['declarations'] = self.declarations + [
            {'name': 'Other', 'signature': 'type Other is (No_Task, Other_Value);'}]
        self.reject(a, message='ambiguous enumeration range bound')

    def test_non_type_bound_links_missing_duplicate_and_unexpected_links(self):
        self.reject(snippet=self.snippet(token='No_Task'))
        a = self.authority()
        _, records = self.restore(a)
        expected = a.required_additions()
        for actual in ([], records * 2, records + [dict(records[0], native_token_start=0)]):
            with self.subTest(actual=actual), self.assertRaisesRegex(ValueError, 'required type-link occurrence'):
                require_complete_additions(actual, expected)
        wrapper = f'<a class="type-reference" href="{HREF}" data-type="{IDENTITY}">Task_Kind</a>'
        self.reject(snippet=self.snippet().replace(wrapper, wrapper * 2))
        self.reject(snippet=self.snippet() + wrapper)


if __name__ == '__main__':
    unittest.main()
