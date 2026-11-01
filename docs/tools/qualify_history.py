#!/usr/bin/env python3
"""Reproduce original historical documentation tests in an isolated Git clone.

Run with the pinned enhancer interpreter and GNATdoc/GNAT environment. Original
failures are retained verbatim with their exit status, not rewritten to pass.
The existing corrected replay is optional and is always identified separately.
No current first-party file is substituted into either checkout.
"""

import argparse
import json
import os
from pathlib import Path
import subprocess
import sys

import ci001r4_replay as replay
import epoch_ci


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
    ci = epoch_ci.ci
    # Every shard is attempted even if an original historical test fails.
    for owner in ci.SHARDS:
        run(owner, [*runner, 'run', '--owner', owner, '--output', str(output / (owner + '.json')),
                    '--artifact-id', '1', '--bundle-sha', ci.file_sha(bundle),
                    '--manifest-sha', ci.digest(manifest)])
    dirty = subprocess.check_output(['git', '-C', str(root), 'status', '--porcelain'])
    if dirty:
        raise ValueError('historical test execution changed immutable source')
    return int(any(result['exit'] for result in results))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--gnatdoc-prefix', type=Path, required=True)
    parser.add_argument('--corrected-replay', action='store_true')
    args = parser.parse_args()
    sys.exit(qualify(args.output, args.gnatdoc_prefix, args.corrected_replay))
