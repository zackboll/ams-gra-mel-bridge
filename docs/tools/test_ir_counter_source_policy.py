"""Independent IR-CI-004 source-policy controls; not an inventory replacement."""

import hashlib
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'scripts'))
import test_doc_source_integrity as historical
from check_doc_source_integrity import PROPOSED_IR_COUNTER_TRANSITION, check

class IRCounterSuccessorTests(unittest.TestCase):
    git = historical.SourceIntegrityTests.git
    write = historical.SourceIntegrityTests.write
    commit = historical.SourceIntegrityTests.commit
    def stage(self, data):
        historical.ProposedIRTransitionTests.stage(self, data)

    def setUp(self):
        historical.ProposedIRTransitionTests.setUp(self)
        self.predecessor = self.states[1]
        self.successor = (ROOT / self.name).read_bytes()
        self.assertEqual(PROPOSED_IR_COUNTER_TRANSITION, (
            self.name, self.corrected_digest,
            'bad90aacb6e19ea19f965c56e07d7856ae1a04696e1a900c90cf91641ba71e32'))
        self.assertEqual(hashlib.sha256(self.successor).hexdigest(),
                         PROPOSED_IR_COUNTER_TRANSITION[2])

    name = 'native/tests/test_ir_stream.c'
    original_digest = '6a2bb092ce231dd794769194476500e9498f50d34bb30f8c290784a76e39d46b'
    corrected_digest = 'fa724b84a4c0ac06e77ffedd4fea81b225e20a0d5a28e12d02eaa2b1a277d400'

    def test_exact_successor_is_proposed_not_approved(self):
        self.stage(self.successor)
        self.assertEqual(check(self.root, self.base)['proposed_ir_test'],
                         'counter_observation_proposed_pending_independent_review')

    def test_successor_disagrees_with_both_historical_states(self):
        for historical in self.states:
            for indexed, working in [(historical, self.successor), (self.successor, historical)]:
                self.stage(indexed)
                self.path.write_bytes(working)
                with self.assertRaisesRegex(ValueError, 'state mismatch'):
                    check(self.root, self.base)

    def test_unknown_intermediate_and_weakened_assertion_rejected(self):
        bad_states = [self.successor + b'\n',
                      self.successor.replace(b'counters.frames_received >= 1 &&', b'1 &&'),
                      self.successor.replace(b'const int closed = close_all(&session, &stream);',
                                             b'const int closed = EXIT_SUCCESS;')]
        for bad in bad_states:
            self.assertNotEqual(bad, self.successor)
            for view in ['working', 'index']:
                self.stage(bad if view == 'index' else self.successor)
                self.path.write_bytes(self.successor if view == 'index' else bad)
                with self.assertRaisesRegex(ValueError, 'unlisted IR-test'):
                    check(self.root, self.base)

    def test_executable_worktree_rejected_for_all_states(self):
        for data in (*self.states, self.successor):
            self.stage(data)
            self.path.chmod(0o755)
            try:
                with self.assertRaisesRegex(ValueError, 'working tree file executable'):
                    check(self.root, self.base)
            finally:
                self.path.chmod(0o644)

    def test_successor_symlink_and_directory_rejected(self):
        self.states = (self.successor,)
        historical.ProposedIRTransitionTests.test_symlink_and_directory_worktree_rejected(self)

    def test_successor_nonregular_and_unmerged_index_rejected(self):
        self.states = (self.successor,)
        historical.ProposedIRTransitionTests.test_nonregular_and_unmerged_index_rejected(self)

    def test_successor_does_not_exempt_unrelated_native_bytes(self):
        self.states = (self.successor,)
        historical.ProposedIRTransitionTests.test_unrelated_native_bytes_still_protected(self)

    def test_successor_cannot_become_foundation(self):
        self.stage(self.successor)
        self.commit()
        with self.assertRaisesRegex(ValueError, 'foundation bytes changed'):
            check(self.root, self.git('rev-parse', 'HEAD').decode().strip())


if __name__ == "__main__":
    unittest.main(verbosity=2)
