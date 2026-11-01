#!/usr/bin/env python3
"""Reproduce original historical documentation tests in an isolated Git clone.

Run with the pinned enhancer interpreter and GNATdoc/GNAT environment. Original
failures are retained verbatim with their exit status, not rewritten to pass.
The existing corrected replay is optional and is always identified separately.
No current first-party file is substituted into either checkout.
"""

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import stat
import subprocess
import sys

import ci001r4_replay as replay
import epoch_ci
import feature_source as source


EVIDENCE = ('inventory.json', 'scripts.json', 'manifest.json')


def verify_execution(root, corrected=False, sealed=None):
    """Verify immutable history plus only the runner's three generated records.

    Existing historical Git ignores still cover generated sites, compiler output
    and bytecode, not qualification/. Never remove or rewrite execution evidence.
    A snapshot taken immediately after sealing must still match after all shards.
    """
    root = Path(root).absolute()
    ci = epoch_ci.ci
    require = ci.require
    require(not any(path.is_symlink() for path in (root, *root.parents)),
            'symlink historical checkout path')
    commit, tree = ((replay.COMMIT, replay.TREE) if corrected else
                    (epoch_ci.HISTORY, epoch_ci.HISTORY_TREE))
    require(source.revision(root, 'HEAD') == commit and
            source.git(root, 'rev-parse', 'HEAD^{tree}').decode().strip() == tree,
            'historical execution commit/tree mismatch')
    objects = source.tree(root, commit)
    expected_index = {name: (mode, oid, '0') for name, (mode, oid) in objects.items()}
    index = {}
    for record in source.git(root, 'ls-files', '--stage', '-z').split(b'\0'):
        if record:
            info, name = record.split(b'\t', 1)
            mode, oid, stage = info.decode().split()
            require(stage == '0', 'unmerged historical index')
            index[name.decode()] = (mode, oid, stage)
    require(index == expected_index, 'historical index differs from immutable tree')
    tracked_hashes = {}
    directories = {'.', 'qualification'}
    for name, (mode, oid) in objects.items():
        path = root / name
        directories.update(str(parent) for parent in Path(name).parents)
        require(mode in {'100644', '100755'} and path.is_file() and not path.is_symlink(),
                'missing/nonregular historical source: ' + name)
        require(not any(parent.is_symlink() for parent in path.parents),
                'symlink historical source ancestor: ' + name)
        require(bool(path.stat().st_mode & 0o111) == (mode == '100755'),
                'historical source mode changed: ' + name)
        data = path.read_bytes()
        require(data == source.git(root, 'cat-file', 'blob', oid),
                'historical tracked source changed: ' + name)
        tracked_hashes[name] = hashlib.sha256(data).hexdigest()

    # Inspect directories as well as Git's untracked files: Git status alone
    # cannot detect an empty unexpected directory or an ignored evidence file.
    def walk_error(error):
        raise error

    observed_directories = set()
    for directory, children, files in os.walk(root, followlinks=False, onerror=walk_error):
        if Path(directory) == root:
            children.remove('.git')
        for name in children + files:
            path = Path(directory) / name
            mode = path.lstat().st_mode
            require(stat.S_ISDIR(mode) or stat.S_ISREG(mode),
                    'symlink/nonregular historical execution path: ' + str(path.relative_to(root)))
        for name in children:
            path = Path(directory) / name
            observed_directories.add(path.relative_to(root).as_posix())
    allowed = {'qualification/' + name for name in EVIDENCE}
    untracked = {name.decode() for name in source.git(
        root, 'ls-files', '--others', '--exclude-standard', '-z').split(b'\0') if name}
    require(untracked == allowed, 'unexpected/missing historical execution evidence or untracked source')
    ignored_files = {name.decode() for name in source.git(
        root, 'ls-files', '--others', '--ignored', '--exclude-standard', '-z').split(b'\0') if name}
    evidence = root / 'qualification'
    require(evidence.is_dir() and not evidence.is_symlink() and
            {path.name for path in evidence.iterdir()} == set(EVIDENCE),
            'unexpected/missing qualification evidence')
    for name in EVIDENCE:
        path = evidence / name
        require(stat.S_ISREG(path.lstat().st_mode), 'nonregular qualification evidence')
    inventory, scripts, manifest = (ci.load(evidence / name) for name in EVIDENCE)
    require(all(isinstance(value, dict) and value.get('schema') == 1
                for value in (inventory, scripts, manifest)), 'malformed qualification evidence')
    require(ci.digest(inventory) == (replay.INVENTORY_SHA256 if corrected else
                                     replay.ORIGINAL_INVENTORY_SHA256),
            'historical evidence inventory mismatch')
    payload = manifest.get('payload')
    require(isinstance(payload, dict) and payload and all(
        isinstance(name, str) and not Path(name).is_absolute() and '..' not in Path(name).parts and
        any(name.startswith(prefix + '/') for prefix in ci.PAYLOAD) and
        isinstance(digest, str) and re.fullmatch(r'[0-9a-f]{64}', digest)
        for name, digest in payload.items()), 'malformed historical evidence payload')
    generated_directories = {'obj', 'obj/docs', 'obj/docs-internal'}
    for name in payload:
        generated_directories.update(parent.as_posix() for parent in Path(name).parents)
    require(all(name in directories or name in generated_directories or
                (Path(name).name == '__pycache__' and Path(name).parent.as_posix() in directories)
                for name in observed_directories), 'unexpected historical execution directory')
    for name in ignored_files:
        path = Path(name)
        require(name in payload or
                (path.parent.name == '__pycache__' and path.suffix == '.pyc' and
                 path.parent.parent.as_posix() in directories),
                'unexpected ignored historical execution file: ' + name)
    identity = {'source_sha': commit, 'source_tree': tree, 'run_id': '1', 'run_attempt': '1',
                'repository': 'zackboll/ams-gra-mel-bridge',
                'qualification_mode': 'local-not-deployable',
                'working_tree_sha256': ci.digest(tracked_hashes)}
    require(manifest['identity'] == identity, 'historical evidence source/run identity mismatch')
    require(manifest['inventory_sha256'] == ci.digest(inventory) and
            manifest['scripts_sha256'] == ci.digest(scripts), 'historical evidence digest mismatch')
    ci.validate_report(scripts, 'scripts', inventory['groups']['scripts'], identity, ci.digest(inventory))
    snapshot = {name: ci.file_sha(evidence / name) for name in EVIDENCE}
    require(sealed is None or snapshot == sealed, 'sealed historical evidence changed during execution')
    return snapshot


def qualify(output, prefix, corrected=False):
    output = Path(output).absolute()
    output.mkdir(parents=True, exist_ok=False)
    root = output / 'source'
    epoch_ci.historical_checkout(root)
    env = {k: v for k, v in os.environ.items() if k != 'PYTHONPATH'}
    env.update(GNATDOC_PREFIX=str(Path(prefix).absolute()), DOC_SOURCE_SHA=epoch_ci.HISTORY,
               GITHUB_RUN_ID='1', GITHUB_RUN_ATTEMPT='1',
               GITHUB_REPOSITORY='zackboll/ams-gra-mel-bridge', DOC_LOCAL_QUALIFICATION='1')
    results = []

    def run(label, command):
        with (output / (label + '.log')).open('wb') as stream:
            status = subprocess.run(command, cwd=root, env=env, stdin=subprocess.DEVNULL,
                                    stdout=stream, stderr=subprocess.STDOUT).returncode
        results.append({'label': label, 'command': command, 'exit': status})
        (output / 'results.json').write_text(json.dumps(results, indent=2) + '\n')
        return status

    for label, command in (
        ('integrity', [sys.executable, 'scripts/check_doc_source_integrity.py']),
        ('generation', ['sh', 'scripts/generate_docs_baseline.sh']),
        ('enhancement', [sys.executable, 'docs/tools/build_enhanced_site.py', 'build']),
        ('verification', [sys.executable, 'docs/tools/build_enhanced_site.py', 'verify']),
    ):
        if run(label, command):
            return 1
    if corrected:
        replay.checkout(root)
        env['DOC_SOURCE_SHA'] = replay.COMMIT
    commit = subprocess.check_output(['git', '-C', str(root), 'rev-parse', 'HEAD'], text=True).strip()
    tree = subprocess.check_output(['git', '-C', str(root), 'rev-parse', 'HEAD^{tree}'], text=True).strip()
    (output / 'identity.json').write_text(json.dumps(
        {'commit': commit, 'tree': tree, 'execution': 'corrected-replay' if corrected else 'original'},
        indent=2) + '\n')
    runner = [sys.executable, 'docs/tools/documentation_ci.py']
    if run('inventory', [*runner, 'inventory']):
        return 1
    if run('scripts', [*runner, 'run', '--owner', 'scripts', '--output', 'qualification/scripts.json']):
        return 1
    bundle = output / 'historical.tar'
    if run('seal', [*runner, 'seal', '--output', str(bundle)]):
        return 1
    manifest = json.loads((root / 'qualification/manifest.json').read_text())
    sealed = verify_execution(root, corrected)
    ci = epoch_ci.ci
    # Every shard is attempted even if an original historical test fails.
    for owner in ci.SHARDS:
        run(owner, [*runner, 'run', '--owner', owner, '--output', str(output / (owner + '.json')),
                    '--artifact-id', '1', '--bundle-sha', ci.file_sha(bundle),
                    '--manifest-sha', ci.digest(manifest)])
    verify_execution(root, corrected, sealed)
    return int(any(result['exit'] for result in results))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--gnatdoc-prefix', type=Path, required=True)
    parser.add_argument('--corrected-replay', action='store_true')
    args = parser.parse_args()
    sys.exit(qualify(args.output, args.gnatdoc_prefix, args.corrected_replay))
