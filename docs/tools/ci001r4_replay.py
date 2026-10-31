"""CI-001R3-R4 explicit corrected historical replay; mechanical acceptance is not approval.

The original POL-002 historical epoch remains immutable and still generates the
authenticated historical publication. Its CI-001 live fixture api_page() took
the first filesystem-enumerated API page; the AMS root namespace page has no
native declaration anchor or type-reference link (hosted run 40989263985).

This policy constructs one exact child commit of the original epoch from
immutable reviewed Git objects only: the DOC-PERF-001-R2 api_page() helper and
that commit's exact live_fixture_source() admission in the historical
qualifier. Historical test execution then identifies itself as this replay,
never as the unmodified original. No candidate file supplies replay bytes.
"""

import ast
import hashlib
import os
from pathlib import Path
import subprocess
import tempfile

HISTORY = '5c136a04ff64d980df7aaa2bd1ee49e1ed80daaf'
HISTORY_TREE = '0073b8627bab4f8409438fdf7102ba5e461fb439'
# DOC-PERF-001-R2: reviewed source-derived live mutation fixture (main ancestry).
REVIEWED = 'd425b29e0570c8c970b318e6c25e0f761a0571e1'
REVIEWED_TREE = '0cc368237ae5a0aace2cda7f84df732d3c1afcf1'
REVIEWED_QUALIFIER_SHA256 = '0df565993ce4858c7c417825f080062d3a21128911e4e45721f897237ab63b5b'
LIVE = 'docs/tests/test_live_docs_site.py'
QUALIFIER = 'docs/tools/documentation_ci.py'
# (original immutable historical bytes, exact corrected replay bytes)
SOURCES = {
    LIVE: ('0658a2e40b8e815e71a9de61e077b7b0a122f74f23ba385d363f80ebfc0b6164',
           '4693fe458c1fae6dffc5d9d115e21dc9a5ff3a74380a0cd16ac22c4181444c74'),
    QUALIFIER: ('87cf203449a6c6489a66bf5a1f9199327a6360ceca85da66e6022daf4bbc4089',
                '93c68f0989e1984a62d468e3bef868bedb4c0eef44b94d2a2b10febe0df8312f'),
}
ADMISSION_SHA256 = '1e5f8b2be10ae80aac73098e46aaad713da4b63686d837558f8aefa9fda259fd'
ANCHOR = b'def validate_inventory(groups, root=None):\n'
CALL_OLD = b'            if name == "docs/tests/test_live_docs_site.py":\n'
CALL_NEW = CALL_OLD + b'                current = live_fixture_source(current)\n'
IDENTITY = {'GIT_AUTHOR_NAME': 'CI-001R3-R4 historical replay',
            'GIT_AUTHOR_EMAIL': 'ci001r3-r4-replay@example.invalid',
            'GIT_AUTHOR_DATE': '1761782400 +0000',
            'GIT_COMMITTER_NAME': 'CI-001R3-R4 historical replay',
            'GIT_COMMITTER_EMAIL': 'ci001r3-r4-replay@example.invalid',
            'GIT_COMMITTER_DATE': '1761782400 +0000'}
MESSAGE = (b'CI-001R3-R4 corrected historical replay of ' + HISTORY.encode() + b'\n\n'
           b'Not the original historical epoch. Only the reviewed DOC-PERF-001-R2\n'
           b'api_page() fixture and its exact live_fixture_source() admission differ.\n')
TREE = 'cf865c6d00d43636641daa84f8ff980fa132df93'
COMMIT = 'f1a512fe0b7de08fff99ed379d6bb2145fe978ef'
# Canonical documentation_ci.digest() of the source-derived inventories. Both
# carry the same 142 script and 437 enhanced identities and shard assignment.
ORIGINAL_INVENTORY_SHA256 = 'f8b4f6aeab9717a37f292454bfe0d74fd888e459b27e8f7b2bf30dea241eebff'
INVENTORY_SHA256 = '8dc2a212e130390e86fb1e39fe468d2fd4cd7d832e7360371c603751c5b0ece6'


def require(condition, message):
    if not condition:
        raise ValueError('CI-001R3-R4 historical replay: ' + message)


def sha(content):
    return hashlib.sha256(content).hexdigest()


def method_range(content, cls, name):
    nodes = [m for n in ast.parse(content).body if isinstance(n, ast.ClassDef) and n.name == cls
             for m in n.body if isinstance(m, ast.FunctionDef) and m.name == name]
    require(len(nodes) == 1, 'missing/duplicate ' + cls + '.' + name)
    return nodes[0].lineno - 1, nodes[0].end_lineno


def restore_helper(successor, original):
    """Only QualifiedArtifactTests.api_page() may differ; every other byte stays."""
    first, last = method_range(successor, 'QualifiedArtifactTests', 'api_page')
    old_first, old_last = method_range(original, 'QualifiedArtifactTests', 'api_page')
    lines = successor.splitlines(keepends=True)
    lines[first:last] = original.splitlines(keepends=True)[old_first:old_last]
    restored = b''.join(lines)
    require(restored == original, 'replay changes more than the api_page() fixture helper')
    return restored


def definition(content, name):
    nodes = [n for n in ast.parse(content).body if isinstance(n, ast.FunctionDef) and n.name == name]
    require(len(nodes) == 1, 'missing/duplicate reviewed ' + name)
    return b''.join(content.splitlines(keepends=True)[nodes[0].lineno - 1:nodes[0].end_lineno])


def successor_sources(blob):
    """Exact replay bytes derived from immutable objects; blob(revision, path)."""
    def read(revision, name):
        try:
            return blob(revision, name)
        except (subprocess.CalledProcessError, OSError, KeyError) as error:
            raise ValueError('CI-001R3-R4 historical replay: missing source ' +
                             revision + ':' + name) from error
    original_live, reviewed_live = read(HISTORY, LIVE), read(REVIEWED, LIVE)
    original_qualifier, reviewed_qualifier = read(HISTORY, QUALIFIER), read(REVIEWED, QUALIFIER)
    require(sha(original_live) == SOURCES[LIVE][0] and
            sha(original_qualifier) == SOURCES[QUALIFIER][0], 'original historical test-source identity')
    require(sha(reviewed_live) == SOURCES[LIVE][1] and
            sha(reviewed_qualifier) == REVIEWED_QUALIFIER_SHA256, 'reviewed DOC-PERF-001-R2 source identity')
    restore_helper(reviewed_live, original_live)
    admission = definition(reviewed_qualifier, 'live_fixture_source')
    require(sha(admission) == ADMISSION_SHA256, 'reviewed live_fixture_source() admission bytes')
    require(reviewed_qualifier.count(CALL_NEW) == 1 and original_qualifier.count(CALL_OLD) == 1 and
            original_qualifier.count(ANCHOR) == 1 and b'live_fixture_source' not in original_qualifier,
            'reviewed/historical qualifier admission point')
    qualifier = original_qualifier.replace(ANCHOR, admission + b'\n\n' + ANCHOR, 1).replace(CALL_OLD, CALL_NEW, 1)
    require(qualifier.replace(admission + b'\n\n', b'', 1).replace(CALL_NEW, CALL_OLD, 1) == original_qualifier,
            'replay qualifier changes more than the reviewed admission')
    result = {LIVE: reviewed_live, QUALIFIER: qualifier}
    require({n: sha(d) for n, d in result.items()} == {n: v[1] for n, v in SOURCES.items()},
            'replay source bytes')
    return result


def git(path, *args, env=None, data=None):
    return subprocess.check_output(['git', '-C', str(path), *args], env=env, input=data,
                                   stderr=subprocess.PIPE)


def build_objects(path):
    """Write replay objects into a disposable clone only; return (tree, commit)."""
    path = Path(path)
    require(git(path, 'rev-parse', HISTORY + '^{tree}').decode().strip() == HISTORY_TREE,
            'original historical epoch tree')
    require(git(path, 'rev-parse', REVIEWED + '^{tree}').decode().strip() == REVIEWED_TREE,
            'reviewed DOC-PERF-001-R2 tree')
    sources = successor_sources(lambda revision, name: git(path, 'cat-file', 'blob', revision + ':' + name))
    env = {k: v for k, v in os.environ.items() if not k.startswith('GIT_')}
    env.update(IDENTITY)
    with tempfile.TemporaryDirectory(prefix='ci001r4-replay-index-') as temp:
        env['GIT_INDEX_FILE'] = str(Path(temp) / 'index')
        git(path, 'read-tree', HISTORY, env=env)
        for name, data in sorted(sources.items()):
            oid = git(path, 'hash-object', '-w', '--stdin', data=data).decode().strip()
            git(path, 'update-index', '--cacheinfo', '100644,' + oid + ',' + name, env=env)
        tree = git(path, 'write-tree', env=env).decode().strip()
    commit = git(path, '-c', 'i18n.commitEncoding=UTF-8', 'commit-tree', '--no-gpg-sign', tree,
                 '-p', HISTORY, env=env, data=MESSAGE).decode().strip()
    return tree, commit


def construct(path):
    tree, commit = build_objects(path)
    require(tree == TREE and commit == COMMIT, 'replay tree/commit identity')
    names = git(path, 'diff-tree', '-r', '--name-only', '--no-commit-id', HISTORY, COMMIT).decode().split()
    require(sorted(names) == sorted(SOURCES), 'replay delta differs from the two reviewed files')
    return commit


def verify_checkout(path):
    """The exact clean committed replay, never a patched original checkout."""
    import feature_source as source
    path = Path(path)
    require(source.revision(path, 'HEAD') == COMMIT, 'checkout is not the corrected replay')
    require(git(path, 'rev-parse', 'HEAD^{tree}').decode().strip() == TREE, 'replay checkout tree')
    source.exact_checkout(path, COMMIT, source.tree(path, COMMIT))
    for name, (_, expected) in SOURCES.items():
        require(sha((path / name).read_bytes()) == expected, 'replay working source ' + name)


def checkout(path):
    """Switch a clean clone of the original epoch to the exact replay commit.

    Ignored generated publication inputs (_site, _site-published, obj) are kept;
    only the two reviewed tracked files change. Identity becomes the replay.
    """
    path = Path(path)
    require(git(path, 'rev-parse', 'HEAD').decode().strip() == HISTORY,
            'replay requires a checkout of the original historical epoch')
    require(not git(path, 'status', '--porcelain', '--untracked-files=all'),
            'original historical checkout is not clean')
    construct(path)
    git(path, 'checkout', '-q', '--detach', COMMIT)
    verify_checkout(path)
    return binding()


def binding():
    """Sealed relationship between the original epoch and its corrected replay."""
    return {'status': 'corrected-replay-not-original-historical-execution',
            'original': {'commit': HISTORY, 'tree': HISTORY_TREE,
                         'test_sources': {n: v[0] for n, v in SOURCES.items()},
                         'inventory_sha256': ORIGINAL_INVENTORY_SHA256},
            'replay': {'commit': COMMIT, 'tree': TREE,
                       'test_sources': {n: v[1] for n, v in SOURCES.items()},
                       'inventory_sha256': INVENTORY_SHA256},
            'reviewed_fixture': {'commit': REVIEWED, 'tree': REVIEWED_TREE,
                                 'admission_sha256': ADMISSION_SHA256},
            'independent_approval': False}


def verify_inventories(original, replay, digest):
    """Replay preserves every original identity and shard; only one source hash."""
    require(digest(original) == ORIGINAL_INVENTORY_SHA256, 'original historical inventory')
    require(digest(replay) == INVENTORY_SHA256, 'corrected replay inventory')
    require(original['groups'] == replay['groups'] and original['shards'] == replay['shards'],
            'replay changed historical test identities or shard assignment')
    require({k: len(v) for k, v in replay['groups'].items()} == {'scripts': 142, 'enhanced': 437} and
            len(set(replay['groups']['scripts'] + replay['groups']['enhanced'])) == 579,
            'replay historical inventory size')
    keys = set(original['test_sources']) | set(replay['test_sources'])
    delta = {k for k in keys if original['test_sources'].get(k) != replay['test_sources'].get(k)}
    require(delta == {LIVE} and original['test_sources'][LIVE] == SOURCES[LIVE][0] and
            replay['test_sources'][LIVE] == SOURCES[LIVE][1], 'replay test-source delta')
    require(original['inherited_sha256'] == replay['inherited_sha256'], 'replay inherited inventory')


# ---------------------------------------------------------------------------
# Exact CI-001R3-R4 source transition. Complete predecessor trees and complete
# successor bytes/modes are bound here; nothing else is authorized.
POLICY = 'docs/tools/ci001r4_replay.py'
MAIN = '219b00c75210f5985d9889e8b541da2e7b658f04'
MAIN_TREE = '4d5c6bb29cfbe6ac39041808e36af34cb84a8c0e'
PREDECESSOR = 'e85d99bbec15452b4edf5ef554784a18986db709'
PREDECESSOR_TREE = '64430578a6e458396561f7888c478634aaed0a8f'
SUCCESSOR = {
 'docs/ci-001r3-r4-historical-replay.md': '34d3dca4d8f41d545545a81641f1b1ecf13f4b42047bdee54c2732b8037daba1',
 'docs/coverage.md': 'aa67dc77830d003c78a9fae9756911c518502866f6c62385418e1cbb6ad165d6',
 'docs/tests/ci001r4_replay_probe.py': '29e5d0c4cc4dffc2b1ca10296072e99666e22ecd34251b453b24116d16eff049',
 'docs/tests/pol002_epoch_tests.py': '37e234e66085b0401cb05e012c68bbe5edc13c3d723162ab79e941ee68bc2d83',
 'docs/tools/epoch_ci.py': '222309935b85510fda83f9007769f8c8fbf3efad0fcf3b4b201dcdf096b7ca09',
}


def source_transition(root, target, candidate, old, new, changed, source):
    if POLICY not in new:
        return set()
    if not any(old.get(name) != new.get(name) for name in set(SUCCESSOR) | {POLICY}):
        return set()
    import doc_ci004_source as integration
    # From main, the complete reviewed DOC-CI-004 successor is also carried;
    # each of those files remains bound to its exact DOC-CI-004 bytes below.
    carried = (set(integration.SUCCESSOR) | {integration.POLICY}) - set(SUCCESSOR)
    for ref, tree in ((MAIN, MAIN_TREE), (PREDECESSOR, PREDECESSOR_TREE)):
        require(source.git(root, 'rev-parse', ref + '^{tree}').decode().strip() == tree,
                'immutable predecessor tree differs: ' + ref)
    require(old in (source.tree(root, MAIN), source.tree(root, PREDECESSOR)),
            'not a complete verified predecessor checkout')
    require(set(changed) <= set(SUCCESSOR) | {POLICY} | carried, 'unrelated source or policy delta')
    require(new.get(POLICY, ('',))[0] == '100644' and
            source.blob(root, candidate, POLICY) == Path(__file__).read_bytes(),
            'substituted executing replay policy')
    require(SUCCESSOR, 'successor identities not frozen')
    for name, expected in SUCCESSOR.items():
        require(new.get(name, ('',))[0] == '100644' and sha(source.blob(root, candidate, name)) == expected,
                'incomplete/unreviewed successor bytes or mode: ' + name)
    for name in ('docs/tests/test_live_docs_site.py', 'docs/tests/test_documentation_ci.py',
                 'docs/tools/documentation_ci.py', 'docs/tools/source_epoch.py'):
        require(new.get(name) == source.tree(root, PREDECESSOR).get(name),
                'protected current test/qualifier/registry changed: ' + name)
    # DOC-CI-004's complete successor bytes remain bound exactly; this replay
    # changes none of them except the reviewed epoch runner and its tests.
    for name, expected in integration.SUCCESSOR.items():
        if name not in SUCCESSOR:
            require(new.get(name, ('',))[0] == '100644' and sha(source.blob(root, candidate, name)) == expected,
                    'DOC-CI-004 successor identity changed: ' + name)
    require(new.get(integration.POLICY, ('',))[0] == '100644' and
            sha(source.blob(root, candidate, integration.POLICY)) ==
            sha(source.blob(root, PREDECESSOR, integration.POLICY)), 'DOC-CI-004 policy changed')
    from ci001r3_workflow import live_contract, preparation_contract, deploy_contract
    for contract in (live_contract, preparation_contract, deploy_contract):
        contract(root)
    return set(SUCCESSOR) | {POLICY} | carried
