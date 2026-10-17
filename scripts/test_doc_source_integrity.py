"""Committed-fixture qualification of DOC-001 source-integrity boundaries."""

import hashlib
import subprocess
import tempfile
import unittest
from pathlib import Path

from check_doc_source_integrity import (
    BASE, ROOT, REVIEWED_NATIVE_TEST_TRANSITION, PROPOSED_IR_TEST_TRANSITION, check,
)


class SourceIntegrityTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.git('init', '-q')
        self.git('config', 'user.name', 'Documentation Fixture')
        self.git('config', 'user.email', 'fixture@example.invalid')
        self.write('native/src/existing.cpp', 'int value = 1;\n')
        self.write('ada/src/demo.ads', 'package Demo is\nend Demo;\n')
        self.commit()
        self.base = self.git('rev-parse', 'HEAD').decode().strip()

    def git(self, *args):
        return subprocess.check_output(['git', '-C', str(self.root), *args],
                                       stderr=subprocess.STDOUT)

    def write(self, name, content):
        path = self.root / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(content)
        return path

    def commit(self):
        self.git('add', '.')
        self.git('commit', '-qm', 'Committed fixture')
        self.git('diff', '--exit-code')
        self.git('diff', '--cached', '--exit-code')
        self.assertEqual(self.git('status', '--porcelain'), b'')

    def test_reject_committed_unauthorized_additions(self):
        additions = {
            'native/src/new.cpp': 'int added = 1;\n',
            'rust/src/new.rs': 'pub fn added() {}\n',
            'python/ams_mel/new.py': 'added = True\n',
            'ada/src/new.ads': 'package New_Unit is end New_Unit;\n',
            'ada/src/new.adb': 'package body New_Unit is end New_Unit;\n',
            'ada/tests/src/new.ads': 'package New_Test is end New_Test;\n',
            'ada/tests/src/new.adb': 'package body New_Test is end New_Test;\n',
            'integration/new.sh': '#!/bin/sh\nexit 0\n',
            '.github/workflows/unexpected.yml': 'name: Unexpected\n',
            'scripts/unrelated.sh': '#!/bin/sh\nexit 0\n',
            'scripts/unrelated.py': 'added = True\n',
            'unrelated.txt': 'Unrelated top-level file.\n',
            'unrelated.gpr': 'project Unrelated is end Unrelated;\n',
            'native/src/line\nbreak.cpp': 'int added = 1;\n',
        }
        for name, content in additions.items():
            with self.subTest(path=name):
                path = self.write(name, content)
                if name.endswith('.sh'):
                    path.chmod(0o755)
                self.commit()
                try:
                    with self.assertRaisesRegex(ValueError, 'unauthorized tracked files added'):
                        check(self.root, self.base)
                finally:
                    path.unlink()
                    self.commit()

    def test_accept_committed_documentation_additions(self):
        for name in ('docs/guide.md', 'docs/toolchain/alire.toml',
                     'docs_api.gpr', 'docs_internal.gpr',
                     'scripts/check_doc_source_integrity.py',
                     'scripts/test_doc_source_integrity.py',
                     'scripts/build_docs_portal.py', 'scripts/test_docs_portal.py',
                     'scripts/generate_docs_baseline.sh',
                     '.github/workflows/documentation.yml'):
            with self.subTest(path=name):
                self.write(name, 'Documentation fixture.\n')
                self.commit()
                self.assertEqual(check(self.root, self.base)['protected_files'], 2)

    def test_existing_production_bytes_remain_protected(self):
        self.write('native/src/existing.cpp', 'int value = 2;\n')
        self.commit()
        with self.assertRaisesRegex(ValueError, 'production bytes changed'):
            check(self.root, self.base)

    def test_ada_comments_allowed_but_tokens_protected(self):
        self.write('ada/src/demo.ads', '-- Documentation.\npackage Demo is\nend Demo;\n')
        self.commit()
        check(self.root, self.base)
        self.write('ada/src/demo.ads', 'package Demo is\n   X : Integer;\nend Demo;\n')
        self.commit()
        with self.assertRaisesRegex(ValueError, 'executable Ada tokens changed'):
            check(self.root, self.base)

    def test_existing_production_removal_rejected(self):
        (self.root / 'native/src/existing.cpp').unlink()
        self.commit()
        with self.assertRaisesRegex(ValueError, 'production file removed'):
            check(self.root, self.base)

    def test_existing_ada_removal_rejected(self):
        (self.root / 'ada/src/demo.ads').unlink()
        self.commit()
        with self.assertRaisesRegex(ValueError, 'Ada source inventory changed'):
            check(self.root, self.base)


class ReviewedNativeTransitionTests(unittest.TestCase):
    # Reuse only fixture operations, not the six original test expectations.
    git = SourceIntegrityTests.git
    write = SourceIntegrityTests.write
    commit = SourceIntegrityTests.commit

    def setUp(self):
        SourceIntegrityTests.setUp(self)
        self.original = {}
        for name, digest, _ in REVIEWED_NATIVE_TEST_TRANSITION:
            data = subprocess.check_output(['git', '-C', str(ROOT), 'show', f'{BASE}:{name}'])
            self.assertEqual(hashlib.sha256(data).hexdigest(), digest)
            path = self.root / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(data)
            self.original[name] = data
        self.write('native/tests/unrelated.c', 'int unrelated = 1;\n')
        self.write('native/vendor/demo.h', '#define DEMO 1\n')
        self.commit()
        self.base = self.git('rev-parse', 'HEAD').decode().strip()
        # Reconstruct the exact reviewed diff without a network request, a
        # mutable manifest, another worktree or PR objects being available.
        self.approved = approved_native_test_bytes(self.original)
        for name, _, digest in REVIEWED_NATIVE_TEST_TRANSITION:
            self.assertEqual(hashlib.sha256(self.approved[name]).hexdigest(), digest)

    def install(self, files, names=None):
        for name in files if names is None else names:
            (self.root / name).write_bytes(files[name])

    def approve(self):
        self.install(self.approved)
        self.commit()

    def test_both_original_files_pass(self):
        self.assertEqual(check(self.root, self.base)['reviewed_native_tests'], 'original')

    def test_both_exact_approved_files_pass(self):
        self.approve()
        self.assertEqual(check(self.root, self.base)['reviewed_native_tests'], 'approved')

    def test_mock_only_replacement_fails(self):
        self.install(self.approved, ['native/tests/mock_rf_provider.cpp'])
        self.commit()
        with self.assertRaisesRegex(ValueError, 'mixed reviewed native-test pair'):
            check(self.root, self.base)

    def test_c_test_only_replacement_fails(self):
        self.install(self.approved, ['native/tests/test_rf_job_interval_status.c'])
        self.commit()
        with self.assertRaisesRegex(ValueError, 'mixed reviewed native-test pair'):
            check(self.root, self.base)

    def test_one_byte_mutation_of_either_approved_file_fails(self):
        self.approve()
        for name, data in self.approved.items():
            with self.subTest(path=name):
                (self.root / name).write_bytes(bytes([data[0] ^ 1]) + data[1:])
                self.commit()
                with self.assertRaisesRegex(ValueError, 'unapproved native-test working tree bytes'):
                    check(self.root, self.base)
                self.install(self.approved)
                self.commit()

    def test_staged_one_byte_mutation_hidden_by_approved_worktree_fails(self):
        self.approve()
        for name, data in self.approved.items():
            with self.subTest(path=name):
                (self.root / name).write_bytes(bytes([data[0] ^ 1]) + data[1:])
                self.git('add', name)
                self.install(self.approved)
                with self.assertRaisesRegex(ValueError, 'unapproved native-test index bytes'):
                    check(self.root, self.base)
                self.git('add', name)

    def test_partial_staged_transition_hidden_by_complete_worktree_fails(self):
        self.install(self.approved)
        for name in self.approved:
            with self.subTest(path=name):
                self.git('add', name)
                with self.assertRaisesRegex(ValueError, 'mixed reviewed native-test pair in index'):
                    check(self.root, self.base)
                (self.root / name).write_bytes(self.original[name])
                self.git('add', name)
                self.install(self.approved)

    def test_complete_worktree_and_index_states_must_agree(self):
        self.install(self.approved)
        with self.assertRaisesRegex(ValueError, 'working tree/index state mismatch'):
            check(self.root, self.base)
        self.commit()
        self.install(self.original)
        with self.assertRaisesRegex(ValueError, 'working tree/index state mismatch'):
            check(self.root, self.base)

    def test_unrelated_protected_sources_still_rejected_in_both_states(self):
        mutations = {
            'native/tests/unrelated.c': ('int unrelated = 2;\n', 'production bytes changed'),
            'native/src/existing.cpp': ('int value = 2;\n', 'production bytes changed'),
            'ada/src/demo.ads': ('package Demo is X : Integer; end Demo;\n',
                                 'executable Ada tokens changed'),
            'native/vendor/demo.h': ('#define DEMO 2\n', 'production bytes changed'),
        }
        for state in ('original', 'approved'):
            if state == 'approved':
                self.approve()
            for name, (text, error) in mutations.items():
                with self.subTest(state=state, path=name):
                    path = self.root / name
                    original = path.read_bytes()
                    path.write_text(text)
                    self.commit()
                    with self.assertRaisesRegex(ValueError, error):
                        check(self.root, self.base)
                    path.write_bytes(original)
                    self.commit()

    def test_unexpected_workflow_addition_still_rejected(self):
        self.approve()
        self.write('.github/workflows/unexpected.yml', 'name: Unexpected\n')
        self.commit()
        with self.assertRaisesRegex(ValueError, 'unauthorized tracked files added'):
            check(self.root, self.base)

    def test_missing_pair_member_rejected_in_worktree_and_index(self):
        self.approve()
        for name in self.approved:
            with self.subTest(path=name):
                (self.root / name).unlink()
                with self.assertRaisesRegex(ValueError, 'working tree file missing'):
                    check(self.root, self.base)
                self.git('rm', '--cached', name)
                self.install(self.approved)
                with self.assertRaisesRegex(ValueError, 'index pair missing'):
                    check(self.root, self.base)
                self.git('add', name)

    def test_symlink_and_unmerged_index_cannot_supply_approved_bytes(self):
        self.approve()
        name = next(iter(self.approved))
        path = self.root / name
        target = self.root / 'docs/approved-copy'
        target.parent.mkdir(parents=True)
        target.write_bytes(path.read_bytes())
        path.unlink()
        path.symlink_to(target)
        with self.assertRaisesRegex(ValueError, 'missing or symlink'):
            check(self.root, self.base)
        path.unlink()
        self.install(self.approved)
        blob = self.git('rev-parse', f'HEAD:{name}').decode().strip()
        self.git('update-index', '--force-remove', name)
        subprocess.run(['git', '-C', str(self.root), 'update-index', '--index-info'],
                       input=f'100644 {blob} 1\t{name}\n'.encode(), check=True)
        with self.assertRaisesRegex(ValueError, 'regular stage-0 file'):
            check(self.root, self.base)

    def test_altered_foundation_does_not_self_authorize(self):
        self.approve()
        with self.assertRaisesRegex(ValueError, 'foundation bytes changed'):
            check(self.root, self.git('rev-parse', 'HEAD').decode().strip())

    def test_command_line_cannot_replace_fixed_foundation(self):
        result = subprocess.run(['python3', str(ROOT / 'scripts/check_doc_source_integrity.py'),
                                 '--base', self.base], capture_output=True, text=True)
        self.assertEqual(result.returncode, 2)
        self.assertIn('invalid choice', result.stderr)

    def test_eligible_path_cannot_be_added_to_unrelated_foundation(self):
        initial = self.git('rev-list', '--max-parents=0', 'HEAD').decode().strip()
        name = 'native/tests/mock_rf_provider.cpp'
        self.write(name, 'Not a reviewed test fixture.\n')
        self.commit()
        # The first fixture commit predates either real test path.
        with self.assertRaisesRegex(ValueError, 'unauthorized tracked files added'):
            check(self.root, initial)

    def test_incomplete_foundation_pair_is_not_an_exemption(self):
        name = 'native/tests/test_rf_job_interval_status.c'
        self.git('rm', name)
        self.commit()
        incomplete = self.git('rev-parse', 'HEAD').decode().strip()
        with self.assertRaisesRegex(ValueError, 'foundation pair incomplete'):
            check(self.root, incomplete)

    def test_nonregular_index_entry_is_not_approved(self):
        self.approve()
        name = next(iter(self.approved))
        blob = self.git('rev-parse', f'HEAD:{name}').decode().strip()
        self.git('update-index', '--cacheinfo', f'120000,{blob},{name}')
        with self.assertRaisesRegex(ValueError, 'regular stage-0 file'):
            check(self.root, self.base)


class ProposedIRTransitionTests(unittest.TestCase):
    git = SourceIntegrityTests.git
    write = SourceIntegrityTests.write
    commit = SourceIntegrityTests.commit
    name = 'native/tests/test_ir_stream.c'
    original_digest = '6a2bb092ce231dd794769194476500e9498f50d34bb30f8c290784a76e39d46b'
    corrected_digest = 'fa724b84a4c0ac06e77ffedd4fea81b225e20a0d5a28e12d02eaa2b1a277d400'

    def setUp(self):
        ReviewedNativeTransitionTests.setUp(self)
        self.assertEqual(PROPOSED_IR_TEST_TRANSITION,
                         (self.name, self.original_digest, self.corrected_digest))
        original = subprocess.check_output(
            ['git', '-C', str(ROOT), 'show', f'{BASE}:{self.name}'])
        self.assertEqual(hashlib.sha256(original).hexdigest(), self.original_digest)
        corrected = corrected_ir_test_bytes(original)
        self.assertEqual(hashlib.sha256(corrected).hexdigest(), self.corrected_digest)
        self.states = (original, corrected)
        self.path = self.root / self.name
        self.path.write_bytes(original)
        self.commit()
        self.base = self.git('rev-parse', 'HEAD').decode().strip()

    def stage(self, data):
        self.path.write_bytes(data)
        self.git('add', self.name)

    def test_each_ir_state_with_each_complete_rf_state(self):
        for rf in (self.original, self.approved):
            ReviewedNativeTransitionTests.install(self, rf)
            self.git('add', 'native/tests')
            for data, expected in zip(self.states, ('original', 'proposed_pending_independent_review')):
                with self.subTest(rf=rf is self.approved, ir=expected):
                    self.stage(data)
                    report = check(self.root, self.base)
                    self.assertEqual(report['proposed_ir_test'], expected)
                    self.assertEqual(report['reviewed_native_tests'],
                                     'approved' if rf is self.approved else 'original')

    def test_mixed_rf_pairs_rejected_with_either_ir_state(self):
        for data in self.states:
            self.stage(data)
            for name in self.original:
                with self.subTest(ir=hashlib.sha256(data).hexdigest(), rf=name):
                    ReviewedNativeTransitionTests.install(self, self.original)
                    (self.root / name).write_bytes(self.approved[name])
                    self.git('add', 'native/tests')
                    with self.assertRaisesRegex(ValueError, 'mixed reviewed native-test pair'):
                        check(self.root, self.base)

    def test_one_byte_and_unlisted_content_rejected_in_either_view(self):
        for data in self.states:
            for bad in (bytes([data[0] ^ 1]) + data[1:], b'Unlisted IR test\n'):
                for view in ('working tree', 'index'):
                    with self.subTest(state=hashlib.sha256(data).hexdigest(), view=view, size=len(bad)):
                        self.stage(data)
                        if view == 'index':
                            self.stage(bad)
                            self.path.write_bytes(data)
                        else:
                            self.path.write_bytes(bad)
                        with self.assertRaisesRegex(ValueError, f'unlisted IR-test {view} bytes'):
                            check(self.root, self.base)

    def test_original_corrected_disagreements_both_directions(self):
        for staged, working in (self.states, self.states[::-1]):
            self.stage(staged)
            self.path.write_bytes(working)
            with self.assertRaisesRegex(ValueError, 'IR-test working tree/index state mismatch'):
                check(self.root, self.base)

    def test_missing_worktree_and_index_rejected(self):
        for data in self.states:
            self.stage(data)
            self.path.unlink()
            with self.assertRaisesRegex(ValueError, 'working tree file missing'):
                check(self.root, self.base)
            self.path.write_bytes(data)
            self.git('rm', '--cached', self.name)
            with self.assertRaisesRegex(ValueError, 'IR-test index file missing'):
                check(self.root, self.base)

    def test_symlink_and_directory_worktree_rejected(self):
        target = self.root / 'docs/ir-copy'
        target.parent.mkdir()
        for data in self.states:
            self.stage(data)
            target.write_bytes(data)
            self.path.unlink()
            self.path.symlink_to(target)
            with self.assertRaisesRegex(ValueError, 'missing or symlink'):
                check(self.root, self.base)
            self.path.unlink()
            self.path.mkdir()
            with self.assertRaisesRegex(ValueError, 'missing or symlink'):
                check(self.root, self.base)
            self.path.rmdir()

    def test_nonregular_and_unmerged_index_rejected(self):
        for data in self.states:
            for mode in ('120000', '100755', '160000', 'unmerged'):
                with self.subTest(state=hashlib.sha256(data).hexdigest(), mode=mode):
                    self.stage(data)
                    blob = self.git('rev-parse', f':{self.name}').decode().strip()
                    if mode == 'unmerged':
                        self.git('update-index', '--force-remove', self.name)
                        subprocess.run(['git', '-C', str(self.root), 'update-index', '--index-info'],
                                       input=f'100644 {blob} 1\t{self.name}\n100644 {blob} 2\t{self.name}\n'.encode(),
                                       check=True)
                    else:
                        oid = self.base if mode == '160000' else blob
                        self.git('update-index', '--cacheinfo', f'{mode},{oid},{self.name}')
                    with self.assertRaisesRegex(ValueError, 'regular stage-0 file'):
                        check(self.root, self.base)
                    self.git('update-index', '--force-remove', self.name)

    def test_foundation_mismatch_for_corrected_and_unlisted_bytes(self):
        for data in (self.states[1], b'Wrong foundation\n'):
            self.stage(data)
            self.commit()
            with self.assertRaisesRegex(ValueError, 'IR-test foundation bytes changed'):
                check(self.root, self.git('rev-parse', 'HEAD').decode().strip())

    def test_ir_path_cannot_be_added_to_unrelated_foundation(self):
        initial = self.git('rev-list', '--max-parents=0', 'HEAD').decode().strip()
        with self.assertRaisesRegex(ValueError, 'unauthorized tracked files added'):
            check(self.root, initial)

    def test_unrelated_native_bytes_still_protected(self):
        for data in self.states:
            self.stage(data)
            for name in ('native/src/existing.cpp', 'native/tests/unrelated.c', 'native/vendor/demo.h'):
                path = self.root / name
                original = path.read_bytes()
                path.write_bytes(original + b'\n')
                with self.assertRaisesRegex(ValueError, 'production bytes changed'):
                    check(self.root, self.base)
                path.write_bytes(original)

    def test_native_additions_still_rejected(self):
        for data in self.states:
            self.stage(data)
            for name in ('native/tests/new.c', 'native/src/new.cpp'):
                path = self.write(name, 'int unauthorized;\n')
                self.git('add', name)
                with self.assertRaisesRegex(ValueError, 'unauthorized tracked files added'):
                    check(self.root, self.base)
                self.git('rm', '-f', name)


def approved_native_test_bytes(original):
    """Literal PR #111 edits; final SHA-256 checks above cover every byte."""
    result = dict(original)

    def replace(name, old, new, count=1):
        before = result[name]
        old, new = old.encode(), new.encode()
        if before.count(old) != count:
            raise AssertionError(f'reviewed fixture context mismatch: {name}')
        result[name] = before.replace(old, new)

    mock = 'native/tests/mock_rf_provider.cpp'
    replace(mock, 'unsigned job_cleanup_destructions{};\n',
            'unsigned job_cleanup_destructions{};\n'
            '/* Test-only identity survives object destruction without retaining the graph. */\n'
            'struct C2OwnerObservation { unsigned shutdowns{}, destructions{}; };\n'
            'std::vector<C2OwnerObservation> c2_owner_observations;\n')
    replace(mock,
            '    explicit MockC2MEL(std::string configuration) : configuration_{std::move(configuration)} {}',
            '    explicit MockC2MEL(std::string configuration) : configuration_{std::move(configuration)}\n'
            '    {\n'
            '        std::lock_guard lock{job_cleanup_mutex};\n'
            '        c2_owner_observations.emplace_back();\n'
            '        owner_ = c2_owner_observations.size();\n'
            '    }')
    replace(mock, '        ++job_cleanup_destructions;\n',
            '        ++job_cleanup_destructions;\n'
            '        ++c2_owner_observations[owner_ - 1].destructions;\n')
    replace(mock, '            ++job_cleanup_shutdowns;\n',
            '            ++job_cleanup_shutdowns;\n'
            '            ++c2_owner_observations[owner_ - 1].shutdowns;\n')
    replace(mock, '    std::string configuration_;\n};\n\n} // namespace',
            '    std::string configuration_;\n    std::size_t owner_{};\n};\n\n} // namespace')
    replace(mock, 'extern "C" __attribute__((visibility("default"))) unsigned mock_rf_job_wait_shutdown_after',
            'extern "C" __attribute__((visibility("default"))) std::size_t mock_rf_c2_last_owner(void)\n'
            '{\n'
            '    std::lock_guard lock{job_cleanup_mutex};\n'
            '    return c2_owner_observations.size();\n'
            '}\n'
            'extern "C" __attribute__((visibility("default"))) unsigned mock_rf_c2_owner_count(\n'
            '    std::size_t owner, unsigned destruction)\n'
            '{\n'
            '    std::lock_guard lock{job_cleanup_mutex};\n'
            '    if (!owner || owner > c2_owner_observations.size()) return std::numeric_limits<unsigned>::max();\n'
            '    const auto& value = c2_owner_observations[owner - 1];\n'
            '    return destruction ? value.destructions : value.shutdowns;\n'
            '}\n'
            'extern "C" __attribute__((visibility("default"))) unsigned mock_rf_job_wait_shutdown_after')
    test = 'native/tests/test_rf_job_interval_status.c'
    replace(test, 'static unsigned (*wait_shutdown)(unsigned);\n',
            'static unsigned (*wait_shutdown)(unsigned);\n'
            'static size_t (*last_c2_owner)(void);\n'
            'static unsigned (*owner_count)(size_t,unsigned);\n')
    replace(test, '    *(void **)(&wait_shutdown)=dlsym(provider,"mock_rf_job_wait_shutdown_after");\n',
            '    *(void **)(&wait_shutdown)=dlsym(provider,"mock_rf_job_wait_shutdown_after");\n'
            '    *(void **)(&last_c2_owner)=dlsym(provider,"mock_rf_c2_last_owner");\n'
            '    *(void **)(&owner_count)=dlsym(provider,"mock_rf_c2_owner_count");\n', count=2)
    replace(test, '    /* No registration yet:',
            '    CHECK(last_c2_owner && owner_count);\n    /* No registration yet:')
    replace(test, '    for (unsigned run=0;run<50;++run) {',
            '    CHECK(last_c2_owner && owner_count);\n    for (unsigned run=0;run<50;++run) {')
    replace(test, '        create_job("c2:finalize-shutdown-throw",&job);\n'
            '        unsigned before=shutdowns();',
            '        create_job("c2:finalize-shutdown-throw",&job);\n'
            '        /* create_job is serialized: identify this C2, not an earlier Finalize\n'
            '         * worker that can still legitimately advance the global counter. */\n'
            '        const size_t owner=last_c2_owner();\n'
            '        unsigned before=owner_count(owner,0);\n'
            '        CHECK(owner && before==0 && owner_count(owner,1)==0);\n'
            '        /* An unrelated C2 shutdown must not satisfy this owner\'s assertion. */\n'
            '        ams_mel_rf_c2 *unrelated=NULL;\n'
            '        CHECK(ams_mel_rf_c2_open(AMS_MEL_TEST_MOCK_RF_PROVIDER,"c2:ok",&unrelated,NULL,0,NULL)==AMS_MEL_OK);\n'
            '        CHECK(ams_mel_rf_c2_close(&unrelated,NULL,0,NULL)==AMS_MEL_OK);\n'
            '        CHECK(owner_count(owner,0)==0 && owner_count(owner,1)==0);')
    replace(test, '        CHECK(shutdowns()==before+1);\n',
            '        CHECK(!job && owner_count(owner,0)==before+1);\n')
    replace(test, '        CHECK(shutdowns()==before+1); /* uncertain graph retained, no retry */',
            '        CHECK(ams_mel_rf_job_close(&job,NULL,0,NULL)==AMS_MEL_OK);\n'
            '        CHECK(owner_count(owner,0)==before+1); /* uncertain graph retained, no retry */\n'
            '        CHECK(owner_count(owner,1)==0 && !has_lifetime("library_unloaded\\n"));')
    return result


def corrected_ir_test_bytes(original):
    """Frozen IR-CI-001 edit of immutable foundation bytes, never live sources."""
    result = original
    edits = (
        (
            (
                '    char base[] = "/tmp/ams-mel-destructor-gap-XXXXXX";\n'
                '    char claimed[PATH_MAX] = "", claim_release[PATH_MAX] = "", claim_arm[PATH_MAX] = "";\n'
                '    char entered[PATH_MAX], unblock[PATH_MAX], waiting[PATH_MAX];\n'
                '    char lifetime[] = "/tmp/ams-mel-destructor-order-XXXXXX";\n'
            ),
            (
                '    char base[] = "/tmp/ams-mel-destructor-gap-XXXXXX";\n'
                '    char claimed[PATH_MAX] = "", claim_release[PATH_MAX] = "", claim_arm[PATH_MAX] = "";\n'
                '    char completed[PATH_MAX] = "", completion_release[PATH_MAX] = "";\n'
                '    char completion_arm[PATH_MAX] = "";\n'
                '    char entered[PATH_MAX], unblock[PATH_MAX], waiting[PATH_MAX];\n'
                '    char lifetime[] = "/tmp/ams-mel-destructor-order-XXXXXX";\n'
            ),
        ),
        (
            (
                '            snprintf(claim_release, sizeof claim_release, "%s.before-detach.release", base) <= 0 ||\n'
                '            snprintf(claim_arm, sizeof claim_arm, "%s.before-detach.arm", base) <= 0 ||\n'
                '            setenv("AMS_MEL_TEST_IMAGE_CLEANUP_BARRIER", base, 1) != 0 ||\n'
                '            !create_marker(claim_arm)) goto cleanup;\n'
                '    }\n'
                '    config.buffer_count = 2;\n'
            ),
            (
                '            snprintf(claim_release, sizeof claim_release, "%s.before-detach.release", base) <= 0 ||\n'
                '            snprintf(claim_arm, sizeof claim_arm, "%s.before-detach.arm", base) <= 0 ||\n'
                '            snprintf(completed, sizeof completed, "%s.snapshot-completion.reached", base) <= 0 ||\n'
                '            snprintf(completion_release, sizeof completion_release, "%s.snapshot-completion.release", base) <= 0 ||\n'
                '            snprintf(completion_arm, sizeof completion_arm, "%s.snapshot-completion.arm", base) <= 0 ||\n'
                '            setenv("AMS_MEL_TEST_IMAGE_CLEANUP_BARRIER", base, 1) != 0 ||\n'
                '            !create_marker(claim_arm) || !create_marker(completion_arm)) goto cleanup;\n'
                '    }\n'
                '    config.buffer_count = 2;\n'
            ),
        ),
        (
            (
                '    if (mutation) {\n'
                "        /* Direct defect evidence: cleanup already owns the channel while B's\n"
                '           destructor is held. Keep teardown paused until destructor reentry\n'
                '           has finished, avoiding intentional invalid provider access. */\n'
                '        for (unsigned i = 0; i < 15000U &&\n'
                '             mock_pool_query("ams_mel_mock_destruction_callbacks_returned") == 0UL; ++i) {\n'
                '            const struct timespec pause = {0, 1000000};\n'
                '            thrd_sleep(&pause, NULL);\n'
                '        }\n'
                '        if (mock_pool_query("ams_mel_mock_destruction_callbacks_returned") != 1UL ||\n'
                '            !create_marker(claim_release)) goto cleanup;\n'
                '    }\n'
                '    stage = 6U;\n'
            ),
            (
                '    if (mutation) {\n'
                "        /* Direct defect evidence: cleanup already owns the channel while B's\n"
                '           destructor is held. Snapshot completion follows release-executor\n'
                '           retirement; the callback counter inside the destructor does not. */\n'
                '        if (!wait_for_marker(completed)) goto cleanup;\n'
                '        if (mock_pool_query("ams_mel_mock_destruction_callbacks_returned") != 1UL ||\n'
                '            !create_marker(claim_release) || !create_marker(completion_release)) goto cleanup;\n'
                '        /* Open both gates before either join: snapshot close can wait for the\n'
                '           cleanup owner, which must no longer be paused at before-detach. */\n'
                '    }\n'
                '    stage = 6U;\n'
            ),
        ),
        (
            (
                '    if (unblock[0]) (void)create_marker(unblock);\n'
                '    if (claim_release[0]) (void)create_marker(claim_release);\n'
                '    if (release_started) (void)thrd_join(release_thread, NULL);\n'
                '    if (close_started) (void)thrd_join(close_thread, NULL);\n'
            ),
            (
                '    if (unblock[0]) (void)create_marker(unblock);\n'
                '    if (claim_release[0]) (void)create_marker(claim_release);\n'
                '    if (completion_release[0]) (void)create_marker(completion_release);\n'
                '    if (release_started) (void)thrd_join(release_thread, NULL);\n'
                '    if (close_started) (void)thrd_join(close_thread, NULL);\n'
            ),
        ),
        (
            (
                '    if (claim_release[0]) (void)unlink(claim_release);\n'
                '    if (claim_arm[0]) (void)unlink(claim_arm);\n'
                '    if (entered[0]) (void)unlink(entered);\n'
                '    if (unblock[0]) (void)unlink(unblock);\n'
            ),
            (
                '    if (claim_release[0]) (void)unlink(claim_release);\n'
                '    if (claim_arm[0]) (void)unlink(claim_arm);\n'
                '    if (completed[0]) (void)unlink(completed);\n'
                '    if (completion_release[0]) (void)unlink(completion_release);\n'
                '    if (completion_arm[0]) (void)unlink(completion_arm);\n'
                '    if (entered[0]) (void)unlink(entered);\n'
                '    if (unblock[0]) (void)unlink(unblock);\n'
            ),
        ),
    )
    for old, new in edits:
        old, new = old.encode(), new.encode()
        if result.count(old) != 1:
            raise AssertionError("frozen IR fixture context mismatch")
        result = result.replace(old, new, 1)
    return result


if __name__ == '__main__':
    unittest.main()
