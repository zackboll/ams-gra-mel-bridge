"""CI-DOC-FAST-001 event fixtures and fail-closed source selection controls."""

import copy
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile
import unittest
from unittest import mock

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'docs/tools'))
import documentation_qualification_source as source
import docs_workflow

HEAVY = ('documentation-search.yml', 'documentation-resize.yml', 'mel-authored-html-pilot.yml')


def workflow(name):
    return (ROOT / '.github/workflows' / name).read_text()


def triggers(text, kind, branch, paths):
    """Bounded reader for the block event/branch/path forms used by these workflows."""
    events = text.split('\non:\n', 1)[1].split('\npermissions:', 1)[0]
    parts = re.split(r'^  ([a-z_]+):\n', events, flags=re.M)
    mapping = dict(zip(parts[1::2], parts[2::2]))
    if kind not in mapping:
        return False
    block = mapping[kind]
    branches = re.findall(r'^    branches: \[([^\]]+)\]$', block, re.M)
    if branches and branch not in [v.strip() for v in branches[0].split(',')]:
        return False
    if '    paths' in block:
        raise AssertionError('unexpected path filtering; extend event fixture reader explicitly')
    return True


class WorkflowTests(unittest.TestCase):
    def test_rust_only_and_other_prs_keep_only_lightweight_workflows(self):
        for path in ('rust/ams-mel/src/lib.rs', 'ada/src/ams.ads',
                     'python/ams_mel/__init__.py', 'native/src/bridge.cpp', 'docs/guide.md'):
            for name in HEAVY:
                with self.subTest(path=path, workflow=name):
                    self.assertFalse(triggers(workflow(name), 'pull_request', 'main', [path]))
            for name in ('ci.yml', 'documentation.yml'):
                self.assertTrue(triggers(workflow(name), 'pull_request', 'main', [path]))

    def test_push_main_and_manual_but_not_other_branches(self):
        for name in HEAVY:
            text = workflow(name)
            with self.subTest(workflow=name):
                self.assertTrue(triggers(text, 'push', 'main', ['rust/src/lib.rs']))
                self.assertFalse(triggers(text, 'push', 'feature', ['docs/guide.md']))
                self.assertTrue(triggers(text, 'workflow_dispatch', 'feature', []))
                self.assertFalse(triggers(text, 'pull_request_target', 'main', []))
                self.assertNotIn('github.event.pull_request', text)
                self.assertIn('  DOC_SOURCE_SHA: ${{ github.sha }}\n', text)
                self.assertIn('          ref: ${{ env.DOC_SOURCE_SHA }}\n', text)
                self.assertIn('          fetch-depth: 0\n', text)
                self.assertIn('          persist-credentials: false\n', text)

    def test_non_deploying_no_failure_bypass_and_separate_concurrency(self):
        for name in HEAVY:
            text = workflow(name)
            with self.subTest(workflow=name):
                self.assertIn('permissions:\n  contents: read\n', text)
                self.assertNotRegex(text, re.compile(r'^\s+[a-z-]+: write\s*$', re.M))
                for forbidden in ('deploy-pages', 'upload-pages-artifact', 'secrets.',
                                  'continue-on-error', '|| true', '--deployment', 'environment:'):
                    self.assertNotIn(forbidden, text)
                self.assertIn("  cancel-in-progress: ${{ github.event_name == 'push' }}\n", text)
                self.assertIn("group: ${{ github.workflow }}-${{ github.event_name }}-${{ github.event_name == 'workflow_dispatch' && github.run_id || github.ref }}", text)
                self.assertIn('        if: always()\n', text)
                self.assertIn('actions/upload-artifact@', text)

    def test_publication_and_weekly_authority_unchanged(self):
        self.assertEqual(set(docs_workflow.check(ROOT)), {'build', 'deploy', 'verify-live'})
        text = workflow('documentation-qualification.yml')
        self.assertIn("    - cron: '23 4 * * 1'", text)
        self.assertTrue(triggers(text, 'workflow_dispatch', 'main', []))
        for event in ('pull_request', 'push'):
            self.assertFalse(triggers(text, event, 'main', ['rust/src/lib.rs']))

    def test_resize_controls_are_source_local_not_cross_commit_content_equality(self):
        text = workflow('documentation-resize.yml')
        self.assertIn('      baseline_sha:\n', text)
        self.assertIn('        required: true\n', text)
        self.assertIn('run: python3 docs/tools/documentation_qualification_source.py', text)
        self.assertIn('git worktree add --detach .doc-tools/resize-baseline-source "$BASE_SOURCE_SHA"', text)
        self.assertIn('--root .doc-tools/resize-baseline-source', text)
        self.assertIn('--candidate .doc-tools/resize-baseline', text)
        self.assertIn('--candidate .doc-tools/resize-current', text)
        self.assertIn('--repeated .doc-tools/resize-repeated', text)
        self.assertNotIn('--baseline ', text)
        for name in ('resize-baseline', 'resize-current', 'resize-repeated'):
            self.assertIn('sha256sum .doc-tools/' + name + '/manifest.json', text)
        self.assertIn('GITHUB_ACTIONS=false .doc-tools/enhancer/bin/python docs/tools/resize_parity.py', text)
        self.assertIn('.doc-tools/resize-source.json', text)
        for command in ('docs/tools/docs_ci.py qualify', 'test_resize_publication.py',
                        'test_enhancer_release.py', 'test_resize_parity.py', 'docs/tools/resize_browser.py',
                        'playwright install --with-deps chromium'):
            self.assertIn(command, text)

    def test_search_and_authored_campaigns_remain_mandatory(self):
        for name, commands in {
            'documentation-search.yml': ('docs/tools/docs_ci.py qualify', 'make test-documentation-source',
                                         'test_search_publication.py', 'docs/tools/search_browser.py'),
            'mel-authored-html-pilot.yml': ('docs/tools/docs_ci.py qualify', 'make test-documentation-source',
                                           'test_authored_html.py', 'docs/tools/qualify_examples.sh',
                                           'docs/tools/qualify_authored_html.py', 'docs/tools/authored_html_browser.py'),
        }.items():
            for command in (*commands, 'playwright install --with-deps chromium'):
                self.assertIn(command, workflow(name))


class SourceFixtures(unittest.TestCase):
    def setUp(self):
        temp = tempfile.TemporaryDirectory()
        self.addCleanup(temp.cleanup)
        self.root = Path(temp.name)
        self.git('init', '-q')
        self.git('config', 'user.name', 'Workflow fixture')
        self.git('config', 'user.email', 'fixture@example.invalid')
        self.base = self.commit('baseline guide')
        self.intermediate = self.commit('legitimate API content change')
        self.head = self.commit('latest source')
        self.event = {'repository': {'full_name': 'owner/repo'}, 'ref': 'refs/heads/main',
                      'before': self.base, 'after': self.head}

    def git(self, *args):
        return source.git(self.root, *args)

    def commit(self, content):
        (self.root / 'guide.md').write_text(content + '\n')
        self.git('add', '.')
        self.git('commit', '-qm', content)
        return self.git('rev-parse', 'HEAD')

    def resolve(self, kind='push', event=None, sha=None, ref='refs/heads/main'):
        return source.resolve(self.root, kind, event or self.event, sha or self.head, ref, 'owner/repo')

    def test_push_uses_before_not_first_parent_for_multi_commit_push(self):
        result = self.resolve()
        self.assertEqual(result['baseline_sha'], self.base)
        self.assertNotEqual(result['baseline_sha'], self.git('rev-parse', 'HEAD^'))
        self.assertEqual(result['source_sha'], self.head)

    def test_manual_explicit_ancestor(self):
        event = {'repository': {'full_name': 'owner/repo'}, 'inputs': {'baseline_sha': self.intermediate}}
        result = self.resolve('workflow_dispatch', event, ref='refs/heads/feature')
        self.assertEqual(result['baseline_sha'], self.intermediate)

    def test_invalid_missing_zero_equal_unavailable_and_option_baselines_fail(self):
        for baseline in (None, '', '0' * 40, self.head, 'f' * 40, 'HEAD^', '--help',
                         self.base.upper(), self.base + '\n'):
            for kind in ('push', 'workflow_dispatch'):
                event = copy.deepcopy(self.event)
                event['before'] = baseline
                event['inputs'] = {'baseline_sha': baseline}
                with self.subTest(kind=kind, baseline=baseline), self.assertRaises(
                        (ValueError, subprocess.CalledProcessError)):
                    self.resolve(kind, event)
        with self.assertRaises(ValueError):
            self.resolve('workflow_dispatch')

    def test_unrelated_baseline_fails(self):
        self.git('checkout', '--orphan', 'unrelated')
        unrelated = self.commit('unrelated source')
        self.git('checkout', '--detach', self.head)
        event = copy.deepcopy(self.event)
        event['before'] = unrelated
        with self.assertRaises(subprocess.CalledProcessError):
            self.resolve(event=event)

    def test_event_and_checkout_mismatch_fail(self):
        for field, value in (('after', self.base), ('ref', 'refs/heads/feature'),
                             ('repository', {'full_name': 'attacker/repo'})):
            event = dict(self.event, **{field: value})
            with self.subTest(field=field), self.assertRaises(ValueError):
                self.resolve(event=event)
        with self.assertRaises(ValueError):
            self.resolve(ref='refs/heads/feature')
        with self.assertRaises(ValueError):
            self.resolve('pull_request')
        self.git('checkout', '--detach', self.base)
        with self.assertRaises(ValueError):
            self.resolve()

    def test_cli_records_source_provenance_and_exports_validated_baseline(self):
        event_path = self.root / 'event.json'
        event_path.write_text(json.dumps(self.event))
        environment = {'GITHUB_EVENT_NAME': 'push', 'GITHUB_EVENT_PATH': str(event_path),
                       'GITHUB_SHA': self.head, 'GITHUB_REF': 'refs/heads/main',
                       'GITHUB_REPOSITORY': 'owner/repo', 'DOC_SOURCE_SHA': self.head,
                       'GITHUB_ENV': str(self.root / 'environment')}
        with mock.patch.dict(os.environ, environment, clear=True), \
                mock.patch.object(source, '__file__', str(self.root / 'docs/tools/source.py')):
            source.main()
            result = json.loads((self.root / '.doc-tools/resize-source.json').read_text())
            self.assertEqual((result['source_sha'], result['baseline_sha']), (self.head, self.base))
            self.assertEqual((self.root / 'environment').read_text(), 'BASE_SOURCE_SHA=' + self.base + '\n')
            os.environ['DOC_SOURCE_SHA'] = self.base
            with self.assertRaisesRegex(ValueError, 'DOC_SOURCE_SHA'):
                source.main()


if __name__ == '__main__':
    unittest.main()
