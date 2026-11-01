#!/usr/bin/env python3
"""Dependency integrity and CI privilege boundaries, not first-party freezes.

Ordinary source, tests and documentation need functional testing and review,
not filename admission or comparisons against historical production bytes.
"""

import hashlib
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[1]


TOKEN = re.compile(
    r'(?P<space>\s+)|(?P<comment>--[^\n]*)|(?P<string>"(?:""|[^"])*")|'
    r"(?P<char>'[^\n]')|(?P<word>[A-Za-z][A-Za-z0-9_]*|[0-9][A-Za-z0-9_#.]*)|"
    r"(?P<op>:=|=>|\.\.|\*\*|/=|<=|>=|<<|>>|<>)|(?P<punct>.)", re.S)


def tokens(text):
    return [match[0] for match in TOKEN.finditer(text)
            if match.lastgroup not in ("space", "comment")]


def native_profile(text):
    result, depth = [], 0
    for token in tokens(text):
        if depth == 0 and token.lower() in ("with", "renames"):
            break
        if token == "(":
            depth += 1
        elif token == ")":
            depth -= 1
        result.append(token)
    while result and result[-1] == ";":
        result.pop()
    return result



def require(condition, message):
    if not condition:
        raise ValueError(message)


def vendor_integrity(root):
    root = Path(root)
    rows = re.findall(r'^([0-9a-f]{64})  (native/vendor/[^\r\n]+)$',
                      (root / 'docs/upstream-files.sha256.md').read_text(), re.M)
    expected = {name: digest for digest, name in rows}
    require(expected and len(expected) == len(rows), 'empty or duplicate vendor checksums')
    vendor = root / 'native/vendor'
    paths = list(vendor.rglob('*'))
    require(not vendor.is_symlink() and not any(p.is_symlink() for p in paths),
            'symlink in vendor dependencies')
    actual = {p.relative_to(root).as_posix() for p in paths if p.is_file()}
    require(actual == set(expected), 'vendor inventory differs from dependency checksums')
    for name, digest in expected.items():
        require('..' not in Path(name).parts, 'unsafe vendor checksum path')
        require(hashlib.sha256((root / name).read_bytes()).hexdigest() == digest,
                'vendor checksum mismatch: ' + name)
    return len(expected)


def workflow_security(root):
    root = Path(root)
    sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'docs/tools'))
    from ci001r3_workflow import deploy_contract, jobs, live_contract, preparation_contract
    for contract in (preparation_contract, deploy_contract, live_contract):
        contract(root)
    for name, job in jobs(root).items():
        require(name == 'deploy' or not re.search(r'^\s+[a-z-]+: write\s*$', job, re.M),
                'write permission outside Pages deployment job: ' + name)
    workflows = root / '.github/workflows'
    names = sorted({*workflows.glob('*.yml'), *workflows.glob('*.yaml')})
    require(names, 'missing workflows')
    for path in names:
        text = path.read_text()
        require('pull_request_target' not in text and 'secrets.' not in text,
                'privileged untrusted workflow: ' + path.name)
        require(not re.search(r'^\s*permissions:\s*(?:write-all|read-all|\{)', text, re.M),
                'ambiguous or broad workflow permissions')
        header = text.split('\njobs:\n', 1)[0]
        require('permissions:\n  contents: read\n' in header,
                'workflow must default to read-only contents')
        if path.name != 'documentation.yml':
            require(not re.search(r'^\s+[a-z-]+: write\s*$', text, re.M),
                    'write permission outside main-only documentation deployment')
            require('actions/deploy-pages@' not in text and 'actions/upload-pages-artifact@' not in text,
                    'Pages outside main-only documentation workflow')
        else:
            events = header.split('on:\n', 1)[1].split('\npermissions:', 1)[0]
            require(events.strip() == 'push:\n    branches: [main]',
                    'documentation must run only on main pushes')
    return len(names)


def check(root=ROOT):
    return {'vendor_files': vendor_integrity(root), 'workflows': workflow_security(root),
            'development': 'ordinary source and tests; no historical byte authorization'}


if __name__ == '__main__':
    print(check())
