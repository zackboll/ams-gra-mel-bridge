"""Exact CI-001R3 source transition; mechanical acceptance is not approval."""

import ast
import hashlib
from pathlib import Path

BASE = 'e4e756aafa4edaffdb11fe341bb016380bf19ca9'
TREE = '41274f8df25f1f3cfd75879778ba760cb84a19e0'
TESTS = {
    'docs/tests/test_live_docs_site.py': (
        '4693fe458c1fae6dffc5d9d115e21dc9a5ff3a74380a0cd16ac22c4181444c74',
        '76937ae947ecb2d3b7b873423abe4cceab93551b05dae281080af45b2f67d076'),
    'docs/tests/test_documentation_ci.py': (
        '5876442d34e4831c92f7df5b882cbbea886ffbec3bfae2731707583a0400973d',
        'b4758385bb330fc17b925aae73592d6128e263d32db50026517dc6502732b236'),
}
METHODS = {
    'docs/tests/test_live_docs_site.py': {
        'test_main_only_read_only_dependency': 'live_contract(ROOT)',
        'test_pr_fixture_discovery_retained': 'preparation_contract(ROOT)'},
    'docs/tests/test_documentation_ci.py': {
        'test_deploy_is_main_only_and_aggregate_authorized': 'deploy_contract(ci.ROOT)'},
}
QUALIFIER = 'docs/tools/documentation_ci.py'
QUALIFIER_HASHES = ('0df565993ce4858c7c417825f080062d3a21128911e4e45721f897237ab63b5b',
                    'b97d8ef68c6ed6788179615439cc91afd3e606200e036b775942f5e740934f0f')
VERIFIER = 'docs/tools/ci001r3_workflow.py'
HISTORICAL_VERIFIER_HASH = '3423a08b0dd496c540cfeb5c4fec1cc2ac9996e87ed17d1bde3610326f46c71d'
VERIFIER_HASH = 'db88551ac6e9ed522e67ddee3075a3e305025df65a877b0fca9abe643d27cee9'


def require(condition, message):
    if not condition:
        raise ValueError('CI-001R3 source transition: ' + message)


def sha(content):
    return hashlib.sha256(content).hexdigest()


def predecessor(name, content, original):
    """Only three reviewed method ranges may differ; retain all other bytes."""
    before, after = ast.parse(original), ast.parse(content)
    replacements = []
    for method, call in METHODS[name].items():
        left = [m for n in before.body if isinstance(n, ast.ClassDef) and n.name == 'WorkflowTests'
                for m in n.body if isinstance(m, ast.FunctionDef) and m.name == method]
        right = [m for n in after.body if isinstance(n, ast.ClassDef) and n.name == 'WorkflowTests'
                 for m in n.body if isinstance(m, ast.FunctionDef) and m.name == method]
        require(len(left) == len(right) == 1, 'missing/renamed/duplicate method ' + method)
        a, b = left[0], right[0]
        function = call.split('(')[0]
        expected = ast.parse(f'def {method}(self):\n    from ci001r3_workflow import {function}\n    {call}\n').body[0]
        require(ast.dump(b) == ast.dump(expected), 'unreviewed replacement method ' + method)
        replacements.append((b.lineno - 1, b.end_lineno, original.splitlines(keepends=True)[a.lineno - 1:a.end_lineno]))
    lines = content.splitlines(keepends=True)
    for first, last, replacement in sorted(replacements, reverse=True):
        lines[first:last] = replacement
    restored = b''.join(lines)
    require(restored == original, 'unaffected helper/import/test bytes changed')
    return restored


def inherited_sources(root, git_bytes):
    """Historical fixtures remain valid, including the existing api_page fix."""
    root = Path(root)
    current = {name: (root / name).read_bytes() for name in TESTS}
    states = [sha(current[name]) == hashes[1] for name, hashes in TESTS.items()]
    # The unchanged CI-001R2 tests construct a minimal historical fixture with
    # immutable CI001 live bytes and the current control module. It is not a
    # committed current candidate (policy_transition still requires both files).
    live_name, controls_name = TESTS
    historical = git_bytes('a0900430d3b2db8ac5dd9d065eaa66aa88ed6751', live_name)
    require(sha(historical) == '0658a2e40b8e815e71a9de61e077b7b0a122f74f23ba385d363f80ebfc0b6164',
            'historical minimal fixture source mismatch')
    if (root != Path(__file__).resolve().parents[2] and current[live_name] == historical and
            sha(current[controls_name]) == TESTS[controls_name][1] and
            not (root / 'docs/tests/test_bootstrap_ci_policy.py').exists() and
            not (root / QUALIFIER).exists()):
        original = git_bytes(BASE, controls_name)
        require(sha(original) == TESTS[controls_name][0], 'minimal control predecessor mismatch')
        return {controls_name: predecessor(controls_name, current[controls_name], original)}
    if not any(states):
        # Older immutable fixtures are qualified by the original checks, not a
        # new blanket hash exemption. Current malformed variants still reject.
        return {}
    require(all(states), 'incomplete three-method transition')
    result = {}
    for name, (old, new) in TESTS.items():
        original = git_bytes(BASE, name)
        require(sha(original) == old and sha(current[name]) == new, 'stale predecessor/successor')
        result[name] = predecessor(name, current[name], original)
    # Both exact independently pinned consumers remain historical authorities;
    # committed policy_transition/diagnostic_transition checks the whole pair.
    for name, expected in ((QUALIFIER, {QUALIFIER_HASHES[1]}),
                           (VERIFIER, {HISTORICAL_VERIFIER_HASH, VERIFIER_HASH})):
        path = root / name
        require(path.is_file() and not path.is_symlink() and not path.stat().st_mode & 0o111 and
                sha(path.read_bytes()) in expected, 'unreviewed correction consumer ' + name)
    return result


def policy_transition(root, target, candidate, old, new, changed, source):
    """Called only by the explicit policy route, not the feature allowlist."""
    paths = set(TESTS) | {QUALIFIER, VERIFIER, 'docs/tools/ci001r3_source.py'}
    if not paths.intersection(changed):
        return set()
    # A policy-only future edit cannot silently reauthorize the protected files.
    if not (set(TESTS) | {QUALIFIER, VERIFIER}).intersection(changed):
        return set()
    require(source.git(root, 'rev-parse', BASE + '^{tree}').decode().strip() == TREE,
            'immutable predecessor tree mismatch')
    for name, hashes in {**TESTS, QUALIFIER: QUALIFIER_HASHES}.items():
        require(name in old and old[name][0] == '100644' and name in new and new[name][0] == '100644',
                'missing/nonregular predecessor or successor ' + name)
        require(sha(source.blob(root, target, name)) == hashes[0] and
                sha(source.blob(root, candidate, name)) == hashes[1], 'unlisted old/new bytes ' + name)
    require(VERIFIER not in old and new.get(VERIFIER, ('',))[0] == '100644' and
            sha(source.blob(root, candidate, VERIFIER)) == HISTORICAL_VERIFIER_HASH, 'unreviewed workflow verifier')
    require(paths <= set(new), 'incomplete corrective source set')
    # The executing proposal remains independently reviewable policy code. It
    # cannot combine the exact test transition with unknown executable surfaces.
    require(all(name in paths or name in {
        'docs/tools/epoch_ci.py', 'docs/tests/pol002_epoch_tests.py',
        'docs/ci-001r3-pol002-workflow-reconciliation.md'} for name in changed),
        'correction mixed with unrelated policy/source')
    historical = source.blob(root, 'e7eaba2cdf61a51f51f5a12d27e246c1041a1ce4', 'docs/tools/ci001r3_source.py')
    require(sha(historical) == '47ecd9449efe60e01783f90aa80ef557c69168ab5ba2b137028401ddc9f52ce1' and
            source.blob(root, candidate, 'docs/tools/ci001r3_source.py') == historical,
            'candidate substituted transition checker')
    for name in TESTS:
        predecessor(name, source.blob(root, candidate, name), source.blob(root, target, name))
    return paths
