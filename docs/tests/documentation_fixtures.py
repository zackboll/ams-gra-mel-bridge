"""Isolated, Git-authenticated historical test inputs; never deployment authority."""

import os
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path[:0] = [str(ROOT / 'docs/tools'), str(ROOT / 'scripts')]
import feature_source as source

# Last five-job documentation workflow before DOC-CI-SIMPLIFY-001. Its own
# contracts and frozen consumer qualify its workflow and historical publication.
HISTORY = '740e15de2b8e1372607173440f479bad332315d4'
TREE = '41ecd4f3c84c1bc9057bd9c3ff2bb9d8cccb3429'


def checkout(destination):
    subprocess.run(['git', 'clone', '-q', '--shared', '--no-checkout',
                    str(ROOT), str(destination)], check=True)
    subprocess.run(['git', '-C', str(destination), 'checkout', '-q', '--detach', HISTORY], check=True)
    verify(destination)
    return Path(destination)


def verify(root):
    if source.git(root, 'rev-parse', 'HEAD^{tree}').decode().strip() != TREE:
        raise ValueError('historical fixture tree mismatch')
    source.exact_checkout(root, HISTORY, source.tree(root, HISTORY))


def execute(root, command):
    """No inherited PYTHONPATH may substitute current modules into frozen tests."""
    environment = dict(os.environ, PYTHONDONTWRITEBYTECODE='1')
    environment.pop('PYTHONPATH', None)
    subprocess.run(command, cwd=root, env=environment, check=True)


def historical_python():
    value = os.environ.get('HISTORICAL_DOC_PYTHON')
    if not value:
        raise ValueError('set HISTORICAL_DOC_PYTHON to the frozen source\'s qualified enhancer interpreter; '
                         'install with its docs/tools/install_enhancer.sh')
    return str(Path(value).absolute())
