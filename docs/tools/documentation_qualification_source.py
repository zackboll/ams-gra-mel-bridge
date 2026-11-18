"""Resolve resize qualification sources from push/manual events, without fallbacks."""

import json
import os
from pathlib import Path
import re
import subprocess


def git(root, *args):
    return subprocess.check_output(['git', '-C', str(root), *args], text=True).strip()


def resolve(root, kind, event, sha, ref, repository):
    if event['repository']['full_name'] != repository:
        raise ValueError('event repository mismatch')
    if kind == 'push':
        if ref != 'refs/heads/main' or event['ref'] != ref or event['after'] != sha:
            raise ValueError('expected exact main push')
        baseline = event['before']
    elif kind == 'workflow_dispatch':
        baseline = event.get('inputs', {}).get('baseline_sha')
    else:
        raise ValueError('expected push or workflow_dispatch')
    for label, value in (('source', sha), ('baseline', baseline)):
        if not isinstance(value, str) or not re.fullmatch(r'[0-9a-f]{40}', value) or value == '0' * 40:
            raise ValueError(label + ' must be a nonzero full lowercase commit SHA')
        if git(root, 'rev-parse', '--verify', value + '^{commit}') != value:
            raise ValueError(label + ' is not the exact commit')
    if git(root, 'rev-parse', 'HEAD') != sha:
        raise ValueError('checkout differs from event source')
    if baseline == sha:
        raise ValueError('baseline must be a strict ancestor of source')
    subprocess.run(['git', '-C', str(root), 'merge-base', '--is-ancestor', baseline, sha], check=True)
    return {'source_sha': sha, 'baseline_sha': baseline, 'event': kind, 'ref': ref,
            'repository': repository, 'parity': 'source-local resize-disabled control and repeated candidate'}


def main():
    root = Path(__file__).resolve().parents[2]
    result = resolve(root, os.environ['GITHUB_EVENT_NAME'],
                     json.loads(Path(os.environ['GITHUB_EVENT_PATH']).read_text()),
                     os.environ['GITHUB_SHA'], os.environ['GITHUB_REF'], os.environ['GITHUB_REPOSITORY'])
    if os.environ['DOC_SOURCE_SHA'] != result['source_sha']:
        raise ValueError('DOC_SOURCE_SHA differs from event source')
    directory = root / '.doc-tools'
    directory.mkdir(exist_ok=True)
    (directory / 'resize-source.json').write_text(json.dumps(result, indent=2) + '\n')
    with open(os.environ['GITHUB_ENV'], 'a') as stream:
        stream.write('BASE_SOURCE_SHA=' + result['baseline_sha'] + '\n')
    print(json.dumps(result))


if __name__ == '__main__':
    main()
