"""CI-003 independent workflow, matrix, and exact-content negative controls."""

import hashlib
import itertools
from pathlib import Path
import re
import subprocess
import sys
import tempfile
from types import SimpleNamespace
import unittest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'scripts'))
import check_doc_source_integrity as integrity

BASELINE = 'e4e756aafa4edaffdb11fe341bb016380bf19ca9'
IDENTITIES = (
    ('.github/workflows/ci.yml',
     'c89a50fab57f9bbb7653cf26a8b0260f72d9436c1ebde188dde4b29f257514f2',
     '1fdeb1545a9fa6820dd247685ec939b1c396101bc3c02b494a60560e400f9244'),
    ('.github/workflows/documentation.yml',
     '979931734ae82a2a98b8684231f06ce17f202c4115eafdf439136c238724a847',
     '234d25c009043f814c4cafa39a05b7b4d311fbf9565661cba8615ed2f370b181'),
)
BLOCK = re.compile(r'^    concurrency:\n(?:^      .+\n)+', re.M)


def instances(name, text):
    result = []
    for match in re.finditer(r'^  ([a-z][a-z-]*):\n(.*?)(?=^  [a-z][a-z-]*:\n|\Z)', text, re.M | re.S):
        job, body = match.groups()
        if '    runs-on:' not in body:
            continue
        blocks = BLOCK.findall(body)
        if len(blocks) != 1 or '      queue: max\n' not in blocks[0] or '      cancel-in-progress: false\n' not in blocks[0]:
            raise ValueError('missing/nonqueueing job lane: ' + job)
        expression = re.search(r'      group: \$\{\{ (.*) \}\}\n', blocks[0])[1]
        axes = re.findall(r'^        (compiler|build_type|shard): \[([^\]]+)\]', body, re.M)
        matrices = [dict(zip([axis for axis, _ in axes], values)) for values in itertools.product(
            *[[v.strip() for v in values.split(',')] for _, values in axes])]
        for matrix in matrices:
            result.append((name, job, matrix, expression))
    return result


def group(instance, event='push', ref='refs/heads/main', run=1, attempt=1):
    _, job, matrix, expression = instance
    # This tiny evaluator handles only the literal expressions used here. It is
    # an independent local model, not proof of GitHub scheduler behavior.
    return eval(expression.replace('&&', ' and ').replace('||', ' or '), {'__builtins__': {}}, {
        'github': SimpleNamespace(event_name=event, ref=ref, run_id=run, run_attempt=attempt, job=job),
        'matrix': SimpleNamespace(**matrix), 'format': lambda fmt, *args: fmt.format(*args)})


class LaneTests(unittest.TestCase):
    def setUp(self):
        self.texts = {name: (ROOT / name).read_text() for name, _, _ in IDENTITIES}
        self.jobs = [job for name, text in self.texts.items() for job in instances(name, text)]

    def test_only_job_concurrency_changes_and_original_checks_preserved(self):
        self.assertEqual([len(instances(name, text)) for name, text in self.texts.items()], [8, 10])
        for name, text in self.texts.items():
            original = subprocess.check_output(['git', '-C', str(ROOT), 'show', BASELINE + ':' + name]).decode()
            self.assertEqual(BLOCK.sub('', text), original)

    def test_three_shared_main_lanes_across_runs_and_attempts(self):
        expected = {'main-runner-lane-1', 'main-runner-lane-2', 'main-runner-lane-3'}
        for run, attempt, event in itertools.product((1, 2), (1, 2), ('push', 'workflow_dispatch')):
            self.assertEqual({group(job, run=run, attempt=attempt, event=event) for job in self.jobs}, expected)
        for name in self.texts:
            self.assertEqual({group(job) for job in self.jobs if job[0] == name}, expected)

    def test_pr_and_non_main_jobs_never_share_groups(self):
        for event, ref in [('pull_request', 'refs/pull/123/merge'), ('pull_request', 'refs/heads/main'),
                           ('workflow_dispatch', 'refs/heads/topic')]:
            groups = [group(job, event, ref, run, attempt) for job in self.jobs
                      for run, attempt in itertools.product((1, 2), (1, 2))]
            self.assertEqual(len(groups), len(set(groups)))
            self.assertTrue(all(g.startswith('unlimited-') for g in groups))

    def test_missing_lane_single_queue_and_cancellation_negative_controls(self):
        for text in self.texts.values():
            for bad in (BLOCK.sub('', text, count=1), text.replace('queue: max', 'queue: single', 1),
                        text.replace('      cancel-in-progress: false', '      cancel-in-progress: true', 1)):
                with self.assertRaisesRegex(ValueError, 'job lane'):
                    instances('fixture', bad)

    def test_four_full_stress_configurations_and_fifty_repeats_unchanged(self):
        native = [job for job in self.jobs if job[1] == 'native']
        self.assertEqual({tuple(sorted(job[2].items())) for job in native}, {
            tuple(sorted({'compiler': compiler, 'build_type': mode}.items()))
            for compiler, mode in itertools.product(('gcc', 'clang'), ('Debug', 'Release'))})
        sys.path.insert(0, str(ROOT / 'docs/tools'))
        import bootstrap_ci_policy as policy
        commands = policy.native_commands(policy.FULL_STRESS)
        self.assertEqual(commands[2], ['ctest', '--test-dir', 'build/native', '--parallel', '4',
                                      '--repeat', 'until-fail:50', '--output-on-failure'])


class WorkflowAuthorizationTests(unittest.TestCase):
    def setUp(self):
        temp = tempfile.TemporaryDirectory(prefix='ci003-policy-')
        self.addCleanup(temp.cleanup)
        self.root = Path(temp.name)
        self.git('init', '-q')
        self.old = {name: subprocess.check_output(['git', '-C', str(ROOT), 'show', BASELINE + ':' + name])
                    for name, _, _ in IDENTITIES}
        self.new = {name: (ROOT / name).read_bytes() for name, _, _ in IDENTITIES}
        self.stage(self.old)

    def git(self, *args):
        return subprocess.check_output(['git', '-C', str(self.root), *args], stderr=subprocess.STDOUT)

    def stage(self, contents):
        for name, data in contents.items():
            path = self.root / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(data)
        self.git('add', '.')

    def state(self):
        return integrity.ci003_workflow_state(self.root, self.git, set(self.old))

    def test_independently_pinned_identities(self):
        self.assertEqual(integrity.CI003_WORKFLOWS, IDENTITIES)
        for name, old, new in IDENTITIES:
            self.assertEqual(hashlib.sha256(self.old[name]).hexdigest(), old)
            self.assertEqual(hashlib.sha256(self.new[name]).hexdigest(), new)
        self.assertEqual(self.state(), 'predecessor')
        self.stage(self.new)
        self.assertEqual(self.state(), 'proposed_pending_independent_review')

    def test_partial_transition_rejected_in_both_directions(self):
        for name in self.old:
            self.stage(self.old)
            self.stage({name: self.new[name]})
            with self.assertRaisesRegex(ValueError, 'mixed or partially'):
                self.state()

    def test_working_index_parity_both_directions(self):
        for indexed, working in ((self.old, self.new), (self.new, self.old)):
            self.stage(indexed)
            for name in self.old:
                (self.root / name).write_bytes(working[name])
                with self.assertRaisesRegex(ValueError, 'state mismatch'):
                    self.state()
                (self.root / name).write_bytes(indexed[name])

    def test_each_workflow_unknown_bytes_in_each_view(self):
        for name in self.old:
            for index in (False, True):
                self.stage(self.new)
                path = self.root / name
                path.write_bytes(self.new[name] + b'# unauthorized\n')
                if index:
                    self.git('add', name)
                    path.write_bytes(self.new[name])
                with self.assertRaisesRegex(ValueError, 'unlisted workflow bytes'):
                    self.state()

    def test_modes_symlinks_missing_and_unmerged(self):
        for name in self.old:
            self.stage(self.new)
            path = self.root / name
            path.chmod(0o755)
            with self.assertRaisesRegex(ValueError, 'not regular'):
                self.state()
            self.git('add', name)
            path.chmod(0o644)
            with self.assertRaisesRegex(ValueError, 'stage-0'):
                self.state()
            self.stage(self.new)
            path.unlink()
            path.symlink_to(ROOT / name)
            with self.assertRaisesRegex(ValueError, 'not regular'):
                self.state()
            path.unlink()
            with self.assertRaisesRegex(ValueError, 'not regular'):
                self.state()
            self.stage(self.new)
            oid = self.git('hash-object', '-w', str(path)).decode().strip()
            self.git('update-index', '--force-remove', name)
            subprocess.run(['git', '-C', str(self.root), 'update-index', '--index-info'],
                           input=f'100644 {oid} 1\t{name}\n100644 {oid} 2\t{name}\n'.encode(), check=True)
            with self.assertRaisesRegex(ValueError, 'stage-0'):
                self.state()
            self.git('update-index', '--force-remove', name)


if __name__ == '__main__':
    unittest.main(verbosity=2)
